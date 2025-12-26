/*
 * CFS Programmer - CYD (ESP32-2432S028) + PN532 (I2C) + TFT_eSPI + XPT2046 touch
 * Firmware: 1.2.0
 *
 * UI updates (Steve request):
 * - Keep portrait orientation (rotation = 2)
 * - Show weight as standard spool sizes only (2kg, 1kg, 750g, 500g, 250g)
 * - Fix header underline break (use full-width header bar instead)
 * - Nudge color swatch + hex left so it doesn't clip the border
 *
 * BLE protocol compatible with macOS app:
 *   GET_VERSION -> "VERSION:x.y.z"
 *   READ        -> "READY", "UID:....", then either "TAG_DATA:..." or "ERROR:..."
 *
 * Hardware (per your config/wiring):
 * - PN532 I2C: SDA=22, SCL=27
 * - Touch: CS=33, SCLK=25, MOSI=32, MISO=39, IRQ=36
 */

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

#include <Adafruit_PN532.h>

#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Adafruit_NeoPixel.h>

// AESLib defines a macro named dump() which collides with ESP32 BLE headers' dump() methods.
// Workaround: include AESLib after BLE headers and undef the macro.
#include <AESLib.h>
#ifdef dump
  #undef dump
#endif

#include <ArduinoJson.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────
// VERSION
// ─────────────────────────────────────────────────────────────
#define FIRMWARE_VERSION "1.2.0"
#define GITHUB_API_URL "https://api.github.com/repos/srobinson9305/cfs-programmer/releases/latest"

// ─────────────────────────────────────────────────────────────
// PINS
// ─────────────────────────────────────────────────────────────
#define I2C_SDA         22
#define I2C_SCL         27

#define WS2812_PIN      4    // CYD onboard RGB LED
#define TFT_BL          21

// Touch (typical CYD; adjust if yours differs)
#define TOUCH_CS        33
#define TOUCH_SCLK      25
#define TOUCH_MOSI      32
#define TOUCH_MISO      39
#define TOUCH_IRQ       36   // If unused, set to 255

// PN532 (I2C)
#define PN532_IRQ       -1
#define PN532_RESET     -1

// ─────────────────────────────────────────────────────────────
// BLE UUIDs
// ─────────────────────────────────────────────────────────────
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TX_CHAR_UUID        "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define RX_CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ─────────────────────────────────────────────────────────────
// GLOBALS
// ─────────────────────────────────────────────────────────────
AESLib aesLib;

TFT_eSPI tft = TFT_eSPI();

// Touch is on its own SPI pins on many CYD boards, so use a dedicated SPIClass.
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
Adafruit_NeoPixel led(1, WS2812_PIN, NEO_GRB + NEO_KHZ800);

BLEServer* pServer = nullptr;
BLECharacteristic* txChar = nullptr;
BLECharacteristic* rxChar = nullptr;
volatile bool bleConnected = false;

// WiFi/OTA settings pushed from mac app
String wifiSSID = "";
String wifiPassword = "";
bool wifiConfigured = false;

// Tag UID
uint8_t currentUID[10];
uint8_t currentUIDLength = 0;

// AES keys (from Creality firmware)
uint8_t u_key[16] = {113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49};
uint8_t d_key[16] = {72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50};

// ─────────────────────────────────────────────────────────────
// STATE (avoid heavy work in BLE callback)
// ─────────────────────────────────────────────────────────────
enum State : uint8_t { STATE_IDLE, STATE_READING, STATE_WRITING, STATE_CHECKING_UPDATE, STATE_UPDATING };
volatile State currentState = STATE_IDLE;

// UI modes for screen
enum UiMode : uint8_t { UI_BOOT, UI_SPLASH, UI_READY, UI_WAIT_TAG, UI_SHOW_TAG, UI_SHOW_ERROR, UI_UPDATING };
volatile UiMode uiMode = UI_BOOT;
volatile bool uiRefreshRequested = false;

// Commands from BLE callback to loop
volatile bool cmdReadRequested = false;
volatile bool cmdCancelRequested = false;
volatile bool cmdCheckUpdateRequested = false;
volatile bool cmdOtaRequested = false;
String pendingOtaURL = "";

// ─────────────────────────────────────────────────────────────
// DISPLAY DATA
// ─────────────────────────────────────────────────────────────
struct TagDisplayData {
  bool valid = false;
  bool blank = false;
  String material = "";
  int lengthMeters = 0;
  int weightGrams = 0;
  String weightLabel = "";
  String colorHex = "FFFFFF"; // without '#'
  String serial = "";
  String message = "";
};

TagDisplayData lastTag;

// ─────────────────────────────────────────────────────────────
// LED
// ─────────────────────────────────────────────────────────────
enum WS2812Color : uint8_t { WS_OFF, WS_RED, WS_GREEN, WS_BLUE, WS_YELLOW, WS_CYAN, WS_MAGENTA };
volatile uint8_t pendingLedRaw = (uint8_t)WS_OFF;

static inline void setLED(WS2812Color color) {
  uint32_t c = 0;
  switch (color) {
    case WS_OFF:     c = led.Color(0, 0, 0); break;
    case WS_RED:     c = led.Color(255, 0, 0); break;
    case WS_GREEN:   c = led.Color(0, 255, 0); break;
    case WS_BLUE:    c = led.Color(0, 0, 255); break;
    case WS_YELLOW:  c = led.Color(255, 255, 0); break;
    case WS_CYAN:    c = led.Color(0, 255, 255); break;
    case WS_MAGENTA: c = led.Color(255, 0, 255); break;
  }
  led.setPixelColor(0, c);
  led.show();
}

static inline void requestLED(WS2812Color color) {
  pendingLedRaw = (uint8_t)color;
}

static inline void applyPendingLed() {
  static WS2812Color last = WS_OFF;
  WS2812Color want = (WS2812Color)pendingLedRaw;
  if (want != last) {
    setLED(want);
    last = want;
  }
}

// ─────────────────────────────────────────────────────────────
// BLE TX helper
// ─────────────────────────────────────────────────────────────
static inline void notifyMac(const String& message) {
  if (bleConnected && txChar) {
    txChar->setValue(message.c_str());
    txChar->notify();
    Serial.print("BLE TX: ");
    Serial.println(message);
  }
}

// ─────────────────────────────────────────────────────────────
// UI helpers
// ─────────────────────────────────────────────────────────────
static inline void drawBorder() {
  tft.drawRect(4, 4, tft.width() - 8, tft.height() - 8, TFT_WHITE);
}

static inline void uiClear() {
  tft.fillScreen(TFT_BLACK);
  drawBorder();
}

static inline void headerBar(const String& title, const String& subtitle) {
  const int barX = 8;
  const int barY = 10;
  const int barW = tft.width() - 16;
  const int barH = 56;
  tft.fillRoundRect(barX, barY, barW, barH, 14, TFT_CYAN);

  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_CYAN);
  tft.drawString(title, tft.width() / 2, barY + 6);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_CYAN);
  tft.drawString(subtitle, tft.width() / 2, barY + 36);
}

static inline void pill(int x, int y, int w, int h, uint16_t fill, uint16_t border) {
  tft.fillRoundRect(x, y, w, h, 10, fill);
  tft.drawRoundRect(x, y, w, h, 10, border);
}

static inline void showSplashScreen() {
  uiClear();
  headerBar("CFS Programmer", "Tap to begin");

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("READY", tft.width()/2, 132);

  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Touchscreen enabled", tft.width()/2, 168);
  tft.drawString("FW v" + String(FIRMWARE_VERSION), tft.width()/2, 196);

  uiMode = UI_SPLASH;
}

static inline void showReadyScreen() {
  uiClear();
  headerBar("CFS Programmer", "Tap the mac app to Read Tag");

  pill(18, 86, tft.width()-36, 40, TFT_DARKGREY, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(String("BLE: ") + (bleConnected ? "Connected" : "Waiting"), tft.width()/2, 106);

  pill(18, 136, tft.width()-36, 40, TFT_DARKGREY, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(String("WiFi: ") + (wifiConfigured ? "Set" : "Not set"), tft.width()/2, 156);

  pill(56, 190, tft.width()-112, 52, TFT_BLUE, TFT_WHITE);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("READY", tft.width()/2, 216);

  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("FW v" + String(FIRMWARE_VERSION), tft.width()/2, tft.height()-10);

  uiMode = UI_READY;
}

static inline void showWaitTagScreen() {
  uiClear();
  headerBar("Read Tag", "Hold steady until complete");
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Place tag on reader...", tft.width()/2, 132);
  uiMode = UI_WAIT_TAG;
}

static inline void showErrorScreen(const String& msg) {
  uiClear();
  headerBar("Error", "Tap OK to dismiss");

  pill(18, 86, tft.width()-36, 120, TFT_DARKGREY, TFT_WHITE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("Read failed:", 30, 100);
  tft.setTextColor(TFT_RED, TFT_DARKGREY);
  tft.drawString(msg, 30, 126);

  pill(70, 222, tft.width()-140, 42, TFT_BLUE, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("OK", tft.width()/2, 243);

  uiMode = UI_SHOW_ERROR;
}

static inline uint16_t hexTo565(const String& hex) {
  String h = hex;
  if (h.startsWith("#")) h.remove(0, 1);
  if (h.length() != 6) return TFT_WHITE;
  uint8_t r = (uint8_t) strtol(h.substring(0,2).c_str(), nullptr, 16);
  uint8_t g = (uint8_t) strtol(h.substring(2,4).c_str(), nullptr, 16);
  uint8_t b = (uint8_t) strtol(h.substring(4,6).c_str(), nullptr, 16);
  return tft.color565(r, g, b);
}

static inline void showTagScreen(const TagDisplayData& td) {
  uiClear();
  headerBar(td.blank ? "Blank Tag" : "Tag Data", "Tap OK to dismiss");

  pill(18, 86, tft.width()-36, 140, TFT_DARKGREY, TFT_WHITE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);

  if (td.blank) {
    tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft.drawString("Blank tag detected", 30, 120);
  } else if (!td.valid) {
    tft.setTextColor(TFT_RED, TFT_DARKGREY);
    tft.drawString(td.message.isEmpty() ? "Unknown tag" : td.message, 30, 120);
  } else {
    const int xL = 30;
    const int xV = 132;

    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.drawString("Material:", xL, 104);
    tft.drawString("Weight:",   xL, 134);
    tft.drawString("Serial:",   xL, 164);
    tft.drawString("Color:",    xL, 194);

    tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft.drawString(td.material, xV, 104);
    tft.drawString(td.weightLabel, xV, 134);
    tft.drawString(td.serial, xV, 164);

    uint16_t swatch = hexTo565(td.colorHex);
    const int swX = xV;     // moved left
    const int swY = 190;
    const int swW = 26;
    const int swH = 16;

    tft.fillRoundRect(swX, swY, swW, swH, 3, swatch);
    tft.drawRoundRect(swX, swY, swW, swH, 3, TFT_WHITE);

    tft.setTextFont(2);
    tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft.drawString("#" + td.colorHex, swX + swW + 10, 192);
  }

  pill(70, 232, tft.width()-140, 42, TFT_BLUE, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("OK", tft.width()/2, 253);

  uiMode = UI_SHOW_TAG;
}

// ─────────────────────────────────────────────────────────────
// TOUCH helpers
// ─────────────────────────────────────────────────────────────
static inline bool touchPressed() {
  if (!touch.touched()) return false;
  delay(25);
  if (!touch.touched()) return false;
  while (touch.touched()) { delay(10); }
  return true;
}

// ─────────────────────────────────────────────────────────────
// TAG math
// ─────────────────────────────────────────────────────────────
static inline float densityForMaterial(const String& material) {
  if (material == "PLA")   return 1.24f;
  if (material == "PETG")  return 1.27f;
  if (material == "ABS")   return 1.04f;
  if (material == "TPU")   return 1.20f;
  if (material == "Nylon") return 1.15f;
  return 1.24f;
}

static inline int metersToGramsApprox(int meters, const String& material) {
  const float d_mm = 1.75f;
  const float radius_m = (d_mm * 0.5f) / 1000.0f;
  const float area_m2 = 3.1415926f * radius_m * radius_m;
  const float dens_kg_m3 = densityForMaterial(material) * 1000.0f;
  const float mass_kg = area_m2 * (float)meters * dens_kg_m3;
  int grams = (int) lroundf(mass_kg * 1000.0f);
  if (grams < 0) grams = 0;
  return grams;
}

static inline int quantizeSpoolWeightGrams(int grams) {
  const int options[] = {250, 500, 750, 1000, 2000};
  int best = options[0];
  int bestDiff = abs(grams - best);
  for (int i = 1; i < 5; i++) {
    int diff = abs(grams - options[i]);
    if (diff < bestDiff) { bestDiff = diff; best = options[i]; }
  }
  return best;
}

static inline String spoolWeightLabel(int gramsStandard) {
  if (gramsStandard >= 1000) return String(gramsStandard / 1000) + "kg";
  return String(gramsStandard) + "g";
}

// ─────────────────────────────────────────────────────────────
// KEY GEN
// ─────────────────────────────────────────────────────────────
static inline void generateKeyFromUID(uint8_t* outputKey) {
  uint8_t uid16[16];
  uint8_t x = 0;
  for (int i = 0; i < 16; i++) {
    if (x >= currentUIDLength) x = 0;
    uid16[i] = currentUID[x++];
  }

  uint8_t bufOut[16];
  byte iv[16] = {0};
  aesLib.encrypt(uid16, 16, bufOut, u_key, 128, iv);
  memcpy(outputKey, bufOut, 6);
}

// ─────────────────────────────────────────────────────────────
// PN532 helpers
// ─────────────────────────────────────────────────────────────
static inline bool waitForTagOnce(uint16_t timeoutMs) {
  return nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUID, &currentUIDLength, timeoutMs);
}

static inline bool authenticateWithRetry(uint8_t block, uint8_t keyNumber, uint8_t* key) {
  const int MAX_RETRIES = 5;
  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    waitForTagOnce(120);
    delay(10);
    if (nfc.mifareclassic_AuthenticateBlock(currentUID, currentUIDLength, block, keyNumber, key)) return true;
    delay(25);
  }
  return false;
}

// ─────────────────────────────────────────────────────────────
// Read tag + populate display
// ─────────────────────────────────────────────────────────────
static inline String readTagAndPopulateDisplay() {
  lastTag = TagDisplayData();

  uint8_t customKey[6];
  generateKeyFromUID(customKey);
  uint8_t defaultKey[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  auto tryAuthSequence = [&](bool slowI2C) -> bool {
    Wire.setClock(slowI2C ? 100000 : 400000);
    delay(5);

    if (authenticateWithRetry(7, 0, defaultKey)) return true;
    if (authenticateWithRetry(7, 0, customKey))  return true;
    if (authenticateWithRetry(7, 1, customKey))  return true;
    if (authenticateWithRetry(7, 1, defaultKey)) return true;

    if (authenticateWithRetry(4, 0, customKey))  return true;
    if (authenticateWithRetry(4, 1, customKey))  return true;

    return false;
  };

  bool authenticated = tryAuthSequence(false);
  if (!authenticated) {
    authenticated = tryAuthSequence(true);
    Wire.setClock(400000);
  }

  if (!authenticated) {
    lastTag.valid = false;
    lastTag.message = "Auth failed - hold tag steady and try again";
    return "ERROR:" + lastTag.message;
  }

  uint8_t raw[48];
  memset(raw, 0, sizeof(raw));

  for (uint8_t blockNum = 4; blockNum <= 6; blockNum++) {
    uint8_t data[16];
    bool ok = false;
    for (int attempt = 0; attempt < 3; attempt++) {
      if (nfc.mifareclassic_ReadDataBlock(blockNum, data)) { ok = true; break; }
      delay(20);
      authenticateWithRetry(7, 0, customKey);
    }
    if (!ok) {
      lastTag.valid = false;
      lastTag.message = "Read failed on block " + String(blockNum);
      return "ERROR:" + lastTag.message;
    }
    memcpy(raw + (blockNum - 4) * 16, data, 16);
    delay(2);
  }

  bool allZero = true, allFF = true;
  for (int i = 0; i < 48; i++) {
    if (raw[i] != 0x00) allZero = false;
    if (raw[i] != 0xFF) allFF = false;
  }
  if (allZero || allFF) {
    lastTag.blank = true;
    lastTag.valid = true;
    lastTag.material = "Blank";
    lastTag.lengthMeters = 0;
    lastTag.weightGrams = 0;
    lastTag.weightLabel = "0g";
    lastTag.colorHex = "CCCCCC";
    lastTag.serial = "N/A";
    return "BLANK_TAG";
  }

  bool needsDecryption = false;
  for (int i = 0; i < 16; i++) {
    if (raw[i] < 0x20 || raw[i] > 0x7E) { needsDecryption = true; break; }
  }

  uint8_t cfs[48];
  if (!needsDecryption) {
    memcpy(cfs, raw, 48);
  } else {
    for (int blockIdx = 0; blockIdx < 3; blockIdx++) {
      byte iv[16] = {0};
      aesLib.decrypt(raw + blockIdx * 16, 16, cfs + blockIdx * 16, d_key, 128, iv);
      delay(1);
    }
  }

  String cfsData;
  cfsData.reserve(48);
  for (int i = 0; i < 48; i++) cfsData += (char)cfs[i];

  if (cfsData.length() != 48) {
    lastTag.valid = false;
    lastTag.message = "Invalid data length";
    return "ERROR:" + lastTag.message;
  }

  String filmID = cfsData.substring(11, 17);
  String color  = cfsData.substring(17, 24);
  String lenHex = cfsData.substring(24, 28);
  String serial = cfsData.substring(28, 34);

  String material = "Unknown";
  if (filmID == "101001" || filmID == "E00003") material = "PLA";
  else if (filmID == "101002") material = "PETG";
  else if (filmID == "101003") material = "ABS";
  else if (filmID == "101004") material = "TPU";
  else if (filmID == "101005") material = "Nylon";

  int lengthMeters = 0;
  for (int i = 0; i < (int)lenHex.length(); i++) {
    char c = lenHex.charAt(i);
    lengthMeters *= 16;
    if (c >= '0' && c <= '9') lengthMeters += (c - '0');
    else if (c >= 'A' && c <= 'F') lengthMeters += (c - 'A' + 10);
    else if (c >= 'a' && c <= 'f') lengthMeters += (c - 'a' + 10);
    else { lengthMeters = 0; break; }
  }

  String colorHex = color;
  if (colorHex.startsWith("0")) colorHex.remove(0, 1);
  colorHex.replace("#", "");
  colorHex.toUpperCase();

  bool looksValid = (material != "Unknown") && (serial.length() == 6) && (colorHex.length() == 6);
  if (!looksValid) {
    lastTag.valid = false;
    lastTag.message = "Not a Creality CFS tag";
    return "ERROR:" + lastTag.message;
  }

  int approx = metersToGramsApprox(lengthMeters, material);
  int stdG = quantizeSpoolWeightGrams(approx);

  lastTag.valid = true;
  lastTag.blank = false;
  lastTag.material = material;
  lastTag.lengthMeters = lengthMeters;
  lastTag.weightGrams = stdG;
  lastTag.weightLabel = spoolWeightLabel(stdG);
  lastTag.colorHex = colorHex;
  lastTag.serial = serial;

  String payload = material + "|" + String(lengthMeters) + "m|#" + colorHex + "|S/N:" + serial;
  return "TAG_DATA:" + payload;
}

// ─────────────────────────────────────────────────────────────
// OTA (optional)
// ─────────────────────────────────────────────────────────────
static inline void otaCheckForUpdate() {
  if (!wifiConfigured) {
    notifyMac("ERROR:WiFi not configured");
    showErrorScreen("WiFi not configured");
    currentState = STATE_IDLE;
    return;
  }

  uiClear();
  headerBar("Updates", "Checking GitHub...");
  uiMode = UI_UPDATING;

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) delay(250);

  if (WiFi.status() != WL_CONNECTED) {
    notifyMac("ERROR:WiFi connection failed");
    showErrorScreen("WiFi connection failed");
    WiFi.disconnect(true);
    currentState = STATE_IDLE;
    return;
  }

  HTTPClient http;
  http.begin(GITHUB_API_URL);
  http.addHeader("User-Agent", "CFS-Programmer");

  int code = http.GET();
  if (code != 200) {
    http.end();
    WiFi.disconnect(true);
    notifyMac("ERROR:GitHub API error");
    showErrorScreen("GitHub API error");
    currentState = STATE_IDLE;
    return;
  }

  String payload = http.getString();
  http.end();
  WiFi.disconnect(true);

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    notifyMac("ERROR:JSON parse error");
    showErrorScreen("JSON parse error");
    currentState = STATE_IDLE;
    return;
  }

  String latest = doc["tag_name"].as<String>();
  latest.replace("fw-v", "");
  latest.replace("v", "");

  if (latest != String(FIRMWARE_VERSION)) {
    String url = doc["assets"][0]["browser_download_url"].as<String>();
    notifyMac("UPDATE_AVAILABLE:" + latest + "," + url);
    uiClear();
    headerBar("Updates", "Update available");
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("v" + latest, tft.width()/2, 140);
    delay(900);
    uiRefreshRequested = true;
  } else {
    notifyMac("UP_TO_DATE");
    uiClear();
    headerBar("Updates", "Up to date");
    delay(900);
    uiRefreshRequested = true;
  }

  currentState = STATE_IDLE;
}

static inline void otaPerformUpdate(const String& firmwareURL) {
  if (!wifiConfigured) {
    notifyMac("ERROR:WiFi not configured");
    showErrorScreen("WiFi not configured");
    currentState = STATE_IDLE;
    return;
  }
  if (firmwareURL.length() < 10) {
    notifyMac("ERROR:Bad firmware URL");
    showErrorScreen("Bad firmware URL");
    currentState = STATE_IDLE;
    return;
  }

  uiClear();
  headerBar("Updating", "Downloading...");
  uiMode = UI_UPDATING;

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) delay(250);

  if (WiFi.status() != WL_CONNECTED) {
    notifyMac("ERROR:WiFi connection failed");
    showErrorScreen("WiFi connection failed");
    WiFi.disconnect(true);
    currentState = STATE_IDLE;
    return;
  }

  HTTPClient http;
  http.begin(firmwareURL);
  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    WiFi.disconnect(true);
    notifyMac("ERROR:Download failed");
    showErrorScreen("Download failed");
    currentState = STATE_IDLE;
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    http.end();
    WiFi.disconnect(true);
    notifyMac("ERROR:Bad content length");
    showErrorScreen("Bad content length");
    currentState = STATE_IDLE;
    return;
  }

  if (!Update.begin(contentLength)) {
    http.end();
    WiFi.disconnect(true);
    notifyMac("ERROR:Not enough space");
    showErrorScreen("Not enough space");
    currentState = STATE_IDLE;
    return;
  }

  WiFiClient *client = http.getStreamPtr();
  size_t written = Update.writeStream(*client);
  http.end();
  WiFi.disconnect(true);

  if (written != (size_t)contentLength) {
    Update.abort();
    notifyMac("ERROR:WriteStream short");
    showErrorScreen("WriteStream short");
    currentState = STATE_IDLE;
    return;
  }

  if (!Update.end() || !Update.isFinished()) {
    notifyMac("ERROR:Update failed");
    showErrorScreen("Update failed");
    currentState = STATE_IDLE;
    return;
  }

  notifyMac("UPDATE_SUCCESS");
  uiClear();
  headerBar("Updating", "Success! Rebooting...");
  delay(900);
  ESP.restart();
}

// ─────────────────────────────────────────────────────────────
// BLE callbacks
// ─────────────────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    bleConnected = true;
    requestLED(WS_CYAN);

    if (txChar) {
      String versionMsg = "VERSION:" + String(FIRMWARE_VERSION);
      txChar->setValue(versionMsg.c_str());
      txChar->notify();
    }

    requestLED(WS_OFF);
    uiRefreshRequested = true;
  }

  void onDisconnect(BLEServer* s) override {
    bleConnected = false;
    requestLED(WS_OFF);
    uiRefreshRequested = true;
    delay(200);
    s->startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    String cmd = String(c->getValue().c_str());
    Serial.print("BLE RX: ");
    Serial.println(cmd);

    if (cmd == "READ") {
      cmdReadRequested = true;
    } else if (cmd == "CANCEL") {
      cmdCancelRequested = true;
    } else if (cmd == "GET_VERSION") {
      notifyMac("VERSION:" + String(FIRMWARE_VERSION));
    } else if (cmd.startsWith("WIFI_CONFIG:")) {
      String config = cmd.substring(12);
      int commaPos = config.indexOf(',');
      if (commaPos > 0) {
        wifiSSID = config.substring(0, commaPos);
        wifiPassword = config.substring(commaPos + 1);
        wifiConfigured = true;
        notifyMac("WIFI_OK");
        uiRefreshRequested = true;
      } else {
        notifyMac("ERROR:Bad WiFi config");
      }
    } else if (cmd == "CHECK_UPDATE") {
      cmdCheckUpdateRequested = true;
    } else if (cmd.startsWith("OTA_UPDATE:")) {
      pendingOtaURL = cmd.substring(11);
      cmdOtaRequested = true;
    }
  }
};

// ─────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(400);

  // LED
  led.begin();
  led.setBrightness(30);
  requestLED(WS_OFF);

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  delay(10);

  // TFT
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(2); // keep portrait
  tft.setTextWrap(false, false);
  uiClear();

  // Touch
  touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  bool touchOK = touch.begin(touchSPI);
  if (touchOK) touch.setRotation(2);

  // PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    requestLED(WS_RED);
    showErrorScreen("PN532 not found");
    while (1) delay(1000);
  }
  nfc.SAMConfig();

  // BLE
  BLEDevice::init("CFS-Programmer");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *svc = pServer->createService(SERVICE_UUID);

  txChar = svc->createCharacteristic(TX_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());

  rxChar = svc->createCharacteristic(RX_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  rxChar->setCallbacks(new RxCallbacks());

  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  // Splash after everything is initialized
  showSplashScreen();
  currentState = STATE_IDLE;
}

// ─────────────────────────────────────────────────────────────
// MAIN LOOP
// ─────────────────────────────────────────────────────────────
void loop() {
  applyPendingLed();

  // Splash dismiss
  if (uiMode == UI_SPLASH && touchPressed()) {
    uiRefreshRequested = true;
  }

  // Refresh UI
  if (uiRefreshRequested) {
    uiRefreshRequested = false;
    showReadyScreen();
  }

  // Cancel
  if (cmdCancelRequested) {
    cmdCancelRequested = false;
    currentState = STATE_IDLE;
    lastTag = TagDisplayData();
    uiRefreshRequested = true;
    requestLED(WS_OFF);
  }

  // Update requests
  if (cmdCheckUpdateRequested) {
    cmdCheckUpdateRequested = false;
    currentState = STATE_CHECKING_UPDATE;
    otaCheckForUpdate();
  }
  if (cmdOtaRequested) {
    cmdOtaRequested = false;
    currentState = STATE_UPDATING;
    otaPerformUpdate(pendingOtaURL);
  }

  // Start read
  if (cmdReadRequested) {
    cmdReadRequested = false;
    currentState = STATE_READING;
    showWaitTagScreen();
    notifyMac("READY");
  }

  // Reading state
  if (currentState == STATE_READING && uiMode == UI_WAIT_TAG) {
    if (waitForTagOnce(60)) {
      String uidStr = "UID:";
      for (uint8_t i = 0; i < currentUIDLength; i++) {
        if (currentUID[i] < 0x10) uidStr += "0";
        uidStr += String(currentUID[i], HEX);
      }
      uidStr.toLowerCase();
      notifyMac(uidStr);

      requestLED(WS_YELLOW);
      uiClear();
      headerBar("Reading", "Please wait...");

      String result = readTagAndPopulateDisplay();

      if (result == "BLANK_TAG") {
        requestLED(WS_CYAN);
        notifyMac("BLANK_TAG");
        showTagScreen(lastTag);
      } else if (result.startsWith("TAG_DATA:")) {
        requestLED(WS_GREEN);
        notifyMac(result);
        showTagScreen(lastTag);
      } else if (result.startsWith("ERROR:")) {
        requestLED(WS_RED);
        notifyMac(result);
        showErrorScreen(result.substring(6));
      } else {
        requestLED(WS_RED);
        notifyMac("ERROR:Unknown result");
        showErrorScreen("Unknown result");
      }

      currentState = STATE_IDLE;
    }
  }

  // If tag/error shown, wait for OK (any touch)
  if (uiMode == UI_SHOW_TAG || uiMode == UI_SHOW_ERROR) {
    if (touchPressed()) {
      uiRefreshRequested = true;
      requestLED(WS_OFF);
    }
  }

  delay(2);
}
