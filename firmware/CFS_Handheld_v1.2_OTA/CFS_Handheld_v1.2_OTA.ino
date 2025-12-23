/*
 * CFS Programmer - ESP32-S3 + PN532
 * Version: 1.2.0
 * 
 * Complete firmware with tag read/write capabilities + OTA
 * 
 * GitHub: srobinson9305/cfs-programmer
 */

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Adafruit_PN532.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include <AESLib.h>
#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════════
// FIRMWARE VERSION
// ═══════════════════════════════════════════════════════════
#define FIRMWARE_VERSION "1.2.0"
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__

// GitHub release check URL
#define GITHUB_API_URL "https://api.github.com/repos/srobinson9305/cfs-programmer/releases/latest"

AESLib aesLib;

// ═══════════════════════════════════════════════════════════
// PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════
#define I2C_SDA         8
#define I2C_SCL         9
#define WS2812_PIN     48
#define PN532_IRQ      -1

// ═══════════════════════════════════════════════════════════
// HARDWARE
// ═══════════════════════════════════════════════════════════
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
Adafruit_PN532 nfc(PN532_IRQ, -1);
Adafruit_NeoPixel led(1, WS2812_PIN, NEO_GRB + NEO_KHZ800);

// ═══════════════════════════════════════════════════════════
// BLE
// ═══════════════════════════════════════════════════════════
BLEServer* pServer = NULL;
BLECharacteristic* txChar = NULL;
BLECharacteristic* rxChar = NULL;
bool bleConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TX_CHAR_UUID        "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define RX_CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ═══════════════════════════════════════════════════════════
// WiFi Credentials (for OTA updates)
// ═══════════════════════════════════════════════════════════
String wifiSSID = "";
String wifiPassword = "";
bool wifiConfigured = false;

// ═══════════════════════════════════════════════════════════
// STATE
// ═══════════════════════════════════════════════════════════
enum State { STATE_IDLE, STATE_READING, STATE_WRITING, STATE_UPDATING };
State currentState = STATE_IDLE;

// ═══════════════════════════════════════════════════════════
// AES KEYS (from Creality firmware)
// ═══════════════════════════════════════════════════════════
uint8_t u_key[16] = {113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49};
uint8_t d_key[16] = {72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50};

// ═══════════════════════════════════════════════════════════
// GLOBAL UID STORAGE
// ═══════════════════════════════════════════════════════════
uint8_t currentUID[7];
uint8_t currentUIDLength = 0;
uint8_t mifareKey[6];

// ═══════════════════════════════════════════════════════════
// WRITE STATE
// ═══════════════════════════════════════════════════════════
String pendingCFSData = "";
int writeTagCount = 0;

// ═══════════════════════════════════════════════════════════
// LED CONTROL
// ═══════════════════════════════════════════════════════════
enum WS2812Color { WS_OFF, WS_RED, WS_GREEN, WS_BLUE, WS_YELLOW, WS_CYAN, WS_MAGENTA };

void setLED(WS2812Color color) {
  switch (color) {
    case WS_OFF:    led.setPixelColor(0, 0, 0, 0); break;
    case WS_RED:    led.setPixelColor(0, 255, 0, 0); break;
    case WS_GREEN:  led.setPixelColor(0, 0, 255, 0); break;
    case WS_BLUE:   led.setPixelColor(0, 0, 0, 255); break;
    case WS_YELLOW: led.setPixelColor(0, 255, 255, 0); break;
    case WS_CYAN:   led.setPixelColor(0, 0, 255, 255); break;
    case WS_MAGENTA:led.setPixelColor(0, 255, 0, 255); break;
  }
  led.show();
  delay(10);
}

// Forward declarations
void showMessage(String line1, String line2, String line3);
void notifyMac(String message);
void checkForUpdate();
void performOTAUpdate(String firmwareURL);
bool waitForTag();
String readTag();
void writeTag();

// ═══════════════════════════════════════════════════════════
// BLE CALLBACKS
// ═══════════════════════════════════════════════════════════
class ServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    Serial.println("✅ BLE Client Connected");
    setLED(WS_CYAN);
    showMessage("Connected!", "Mac app ready", "");
    delay(500);

    // ⭐ SEND FIRMWARE VERSION IMMEDIATELY
    String versionMsg = "VERSION:" + String(FIRMWARE_VERSION);
    if (txChar) {
      txChar->setValue(versionMsg.c_str());
      txChar->notify();
      Serial.print("📤 Sent version: ");
      Serial.println(FIRMWARE_VERSION);
    }

    delay(100);
    setLED(WS_OFF);
    showMessage("Ready!", "v" + String(FIRMWARE_VERSION), "Waiting...");
  }

  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    Serial.println("❌ BLE Client Disconnected");
    showMessage("Disconnected", "Waiting", "");
    delay(500);
    pServer->startAdvertising();
    Serial.println("🔄 Restarted BLE advertising");
  }
};

class RxCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String cmd = pCharacteristic->getValue();
    Serial.print("BLE RX: ");
    Serial.println(cmd);

    if (cmd == "READ") {
      currentState = STATE_READING;
      showMessage("Place tag", "to read", "");
      notifyMac("READY");

    } else if (cmd.startsWith("WRITE:")) {
      currentState = STATE_WRITING;
      pendingCFSData = cmd.substring(6);
      writeTagCount = 0;
      Serial.print("Writing CFS data: ");
      Serial.println(pendingCFSData);
      showMessage("Writing...", "Tag 1 of 2", "");
      notifyMac("WRITE_READY");

    } else if (cmd == "GET_VERSION") {
      notifyMac("VERSION:" + String(FIRMWARE_VERSION));

    } else if (cmd.startsWith("WIFI_CONFIG:")) {
      String config = cmd.substring(12);
      int commaPos = config.indexOf(',');
      if (commaPos > 0) {
        wifiSSID = config.substring(0, commaPos);
        wifiPassword = config.substring(commaPos + 1);
        wifiConfigured = true;
        Serial.println("WiFi configured: " + wifiSSID);
        notifyMac("WIFI_OK");
      }

    } else if (cmd == "CHECK_UPDATE") {
      checkForUpdate();

    } else if (cmd.startsWith("OTA_UPDATE:")) {
      String firmwareURL = cmd.substring(11);
      performOTAUpdate(firmwareURL);

    } else if (cmd == "CANCEL") {
      currentState = STATE_IDLE;
      writeTagCount = 0;
      pendingCFSData = "";
      setLED(WS_OFF);
      showMessage("Cancelled", "", "");
    }
  }
};

void notifyMac(String message) {
  if (bleConnected && txChar) {
    txChar->setValue(message.c_str());
    txChar->notify();
    Serial.print("BLE TX: ");
    Serial.println(message);
  }
}

// ═══════════════════════════════════════════════════════════
// KEY GENERATION FROM UID (using correct AESLib API)
// ═══════════════════════════════════════════════════════════
void generateKeyFromUID(uint8_t* outputKey) {
  Serial.println("🔐 Generating MIFARE key from UID...");

  // Create 16-byte buffer by repeating UID
  uint8_t uid16[16];
  int x = 0;
  for (int i = 0; i < 16; i++) {
    if (x >= currentUIDLength) x = 0;
    uid16[i] = currentUID[x];
    x++;
  }

  // Encrypt with u_key using AESLib API: encrypt(input, inputLen, output, key, bits, iv)
  uint8_t bufOut[16];
  byte iv[16] = {0};  // Zero IV
  aesLib.encrypt(uid16, 16, bufOut, u_key, 128, iv);

  // Use first 6 bytes as MIFARE key
  memcpy(outputKey, bufOut, 6);

  Serial.print("   MIFARE Key: ");
  for (int i = 0; i < 6; i++) {
    if (outputKey[i] < 0x10) Serial.print("0");
    Serial.print(outputKey[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// ═══════════════════════════════════════════════════════════
// AUTHENTICATION WITH RETRY
// ═══════════════════════════════════════════════════════════
bool authenticateWithRetry(uint8_t block, uint8_t keyNumber, uint8_t* key) {
  const int MAX_RETRIES = 3;

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    if (attempt > 1) {
      Serial.print("      Retry ");
      Serial.print(attempt);
      Serial.print("/");
      Serial.print(MAX_RETRIES);
      Serial.print("... ");

      // Re-select the tag
      delay(100);
      uint8_t tempUID[7];
      uint8_t tempLen;
      nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tempUID, &tempLen, 50);
      delay(50);
    }

    if (nfc.mifareclassic_AuthenticateBlock(currentUID, currentUIDLength, block, keyNumber, key)) {
      if (attempt > 1) Serial.println("✅");
      return true;
    }

    if (attempt > 1) Serial.println("❌");
    delay(100);
  }

  return false;
}

// ═══════════════════════════════════════════════════════════
// WAIT FOR TAG
// ═══════════════════════════════════════════════════════════
bool waitForTag() {
  bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUID, &currentUIDLength, 100);

  if (!success) {
    return false;
  }

  Serial.println("\n═══════════════════════════════════════");
  Serial.print("✅ TAG DETECTED! UID (");
  Serial.print(currentUIDLength);
  Serial.print(" bytes): ");
  for (uint8_t i = 0; i < currentUIDLength; i++) {
    if (currentUID[i] < 0x10) Serial.print("0");
    Serial.print(currentUID[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("═══════════════════════════════════════");

  String uidStr = "UID:";
  for (uint8_t i = 0; i < currentUIDLength; i++) {
    if (currentUID[i] < 0x10) uidStr += "0";
    uidStr += String(currentUID[i], HEX);
  }
  notifyMac(uidStr);

  return true;
}

// ═══════════════════════════════════════════════════════════
// TAG READING (using correct AESLib API)
// ═══════════════════════════════════════════════════════════
String readTag() {
  Serial.println("📖 Reading Creality CFS tag...");

  uint8_t customKey[6];
  generateKeyFromUID(customKey);

  Serial.println("🔑 Authenticating (with retry logic)...");

  uint8_t defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool authenticated = false;

  // Try 1: Default Key A
  Serial.print("   [1] Default Key A on Block 7... ");
  if (authenticateWithRetry(7, 0, defaultKey)) {
    Serial.println("✅");
    authenticated = true;
  } else {
    Serial.println("❌");

    // Try 2: Custom Key A
    Serial.print("   [2] Custom Key A on Block 7... ");
    if (authenticateWithRetry(7, 0, customKey)) {
      Serial.println("✅ SUCCESS!");
      authenticated = true;
    } else {
      Serial.println("❌");

      // Try 3: Custom Key B
      Serial.print("   [3] Custom Key B on Block 7... ");
      if (authenticateWithRetry(7, 1, customKey)) {
        Serial.println("✅ SUCCESS!");
        authenticated = true;
      } else {
        Serial.println("❌");

        // Try 4: Direct to Block 4
        Serial.print("   [4] Custom Key A on Block 4... ");
        if (authenticateWithRetry(4, 0, customKey)) {
          Serial.println("✅ SUCCESS!");
          authenticated = true;
        } else {
          Serial.println("❌");
          Serial.println("❌ AUTHENTICATION FAILED");
          Serial.println("   Did you add #define SLOWDOWN 1?");
          return "ERROR:Auth failed - check library patch";
        }
      }
    }
  }

  if (!authenticated) {
    return "ERROR:Authentication failed";
  }

  // Read blocks 4, 5, 6
  Serial.println("📄 Reading data blocks...");

  String cfsData = "";
  for (uint8_t blockNum = 4; blockNum <= 6; blockNum++) {
    uint8_t data[16];

    Serial.print("   Block ");
    Serial.print(blockNum);
    Serial.print(": ");

    bool readSuccess = false;
    for (int attempt = 0; attempt < 3; attempt++) {
      if (attempt > 0) {
        delay(50);
        authenticateWithRetry(7, 0, customKey);
      }

      if (nfc.mifareclassic_ReadDataBlock(blockNum, data)) {
        readSuccess = true;
        break;
      }
    }

    if (!readSuccess) {
      Serial.println("❌ READ FAILED");
      return "ERROR:Read failed on block " + String(blockNum);
    }

    for (int i = 0; i < 16; i++) {
      if (data[i] < 0x10) Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println("✅");

    for (int i = 0; i < 16; i++) {
      cfsData += (char)data[i];
    }
  }

  // Check if encrypted
  bool needsDecryption = false;
  for (int i = 0; i < min(16, (int)cfsData.length()); i++) {
    uint8_t c = cfsData.charAt(i);
    if (c < 0x20 || c > 0x7E) {
      needsDecryption = true;
      break;
    }
  }

  // Decrypt if needed (using correct AESLib API)
  if (needsDecryption) {
    Serial.println("🔓 Decrypting with d_key...");

    String decrypted = "";
    for (int blockIdx = 0; blockIdx < 3; blockIdx++) {
      uint8_t encBlock[16];
      uint8_t decBlock[16];

      for (int i = 0; i < 16; i++) {
        encBlock[i] = (uint8_t)cfsData.charAt(blockIdx * 16 + i);
      }

      // decrypt(input, inputLen, output, key, bits, iv)
      byte iv[16] = {0};
      aesLib.decrypt(encBlock, 16, decBlock, d_key, 128, iv);

      for (int i = 0; i < 16; i++) {
        decrypted += (char)decBlock[i];
      }
    }

    cfsData = decrypted;
  }

  if (cfsData.length() != 48) {
    return "ERROR:Invalid data length";
  }

  // Parse CFS format
  String filmID = cfsData.substring(11, 17);
  String color = cfsData.substring(17, 24);
  String length = cfsData.substring(24, 28);
  String serial = cfsData.substring(28, 34);

  String material = "Unknown";
  if (filmID == "101001" || filmID == "E00003") material = "PLA";
  else if (filmID == "101002") material = "PETG";
  else if (filmID == "101003") material = "ABS";
  else if (filmID == "101004") material = "TPU";
  else if (filmID == "101005") material = "Nylon";

  int lengthMeters = 0;
  for (int i = 0; i < length.length(); i++) {
    char c = length.charAt(i);
    lengthMeters = lengthMeters * 16;
    if (c >= '0' && c <= '9') lengthMeters += (c - '0');
    else if (c >= 'A' && c <= 'F') lengthMeters += (c - 'A' + 10);
    else if (c >= 'a' && c <= 'f') lengthMeters += (c - 'a' + 10);
  }

  Serial.println("✅ TAG READ COMPLETE!");
  Serial.print("   Material:  ");
  Serial.println(material);
  Serial.print("   Length:    ");
  Serial.print(lengthMeters);
  Serial.println(" meters");
  Serial.print("   Color:     #");
  Serial.println(color.substring(1));

  return material + "|" + String(lengthMeters) + "m|#" + color.substring(1) + "|S/N:" + serial;
}

// ═══════════════════════════════════════════════════════════
// TAG WRITING (using correct AESLib API)
// ═══════════════════════════════════════════════════════════
void writeTag() {
  if (pendingCFSData.length() != 96) {
    Serial.println("❌ Invalid CFS data length");
    notifyMac("ERROR:Invalid data length");
    currentState = STATE_IDLE;
    return;
  }

  setLED(WS_BLUE);

  if (writeTagCount == 0) {
    showMessage("Tag 1 of 2", "Place tag", "");
  } else {
    showMessage("Tag 2 of 2", "Place tag", "");
  }

  if (!waitForTag()) {
    return; // No tag present
  }

  Serial.println("✅ Tag " + String(writeTagCount + 1) + " detected");

  // Generate MIFARE key
  uint8_t customKey[6];
  generateKeyFromUID(customKey);

  // Authenticate
  bool authenticated = authenticateWithRetry(4, 0, customKey);

  if (!authenticated) {
    Serial.println("❌ Auth failed");
    notifyMac("ERROR:Auth failed");
    currentState = STATE_IDLE;
    return;
  }

  // Convert hex string to bytes
  uint8_t cfsBytes[48];
  for (int i = 0; i < 48; i++) {
    String byteStr = pendingCFSData.substring(i * 2, i * 2 + 2);
    cfsBytes[i] = strtol(byteStr.c_str(), NULL, 16);
  }

  // Encrypt the data (using correct AESLib API)
  uint8_t encrypted[48];
  byte iv[16] = {0};

  for (int i = 0; i < 3; i++) {
    aesLib.encrypt(cfsBytes + (i * 16), 16, encrypted + (i * 16), u_key, 128, iv);
  }

  // Write blocks 4, 5, 6
  bool writeSuccess = true;
  for (int block = 4; block <= 6; block++) {
    Serial.print("✍️ Writing block ");
    Serial.print(block);
    Serial.print("...");

    if (nfc.mifareclassic_WriteDataBlock(block, encrypted + ((block - 4) * 16))) {
      Serial.println(" ✅");
    } else {
      Serial.println(" ❌");
      writeSuccess = false;
      break;
    }
  }

  if (!writeSuccess) {
    notifyMac("ERROR:Write failed");
    setLED(WS_RED);
    currentState = STATE_IDLE;
    return;
  }

  writeTagCount++;

  if (writeTagCount == 1) {
    setLED(WS_GREEN);
    notifyMac("TAG1_WRITTEN");
    Serial.println("✅ Tag 1 complete!");
    delay(1000);
    showMessage("Tag 1 OK!", "Place Tag 2", "");
    setLED(WS_BLUE);
  } else {
    setLED(WS_GREEN);
    notifyMac("TAG2_WRITTEN");
    Serial.println("✅ Tag 2 complete! Both tags written!");
    showMessage("Complete!", "Both tags OK", "");
    delay(2000);
    currentState = STATE_IDLE;
    writeTagCount = 0;
    pendingCFSData = "";
    setLED(WS_OFF);
    showMessage("Ready!", "Waiting", "");
  }
}

// ═══════════════════════════════════════════════════════════
// OTA UPDATE FUNCTIONS (same as before)
// ═══════════════════════════════════════════════════════════
void checkForUpdate() {
  if (!wifiConfigured) {
    notifyMac("ERROR:WiFi not configured");
    return;
  }

  Serial.println("\n🔍 Checking for firmware updates...");
  showMessage("Checking...", "for updates", "");

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ WiFi connection failed");
    notifyMac("ERROR:WiFi connection failed");
    showMessage("WiFi Error", "Check settings", "");
    WiFi.disconnect();
    return;
  }

  Serial.println("\n✅ WiFi connected");

  HTTPClient http;
  http.begin(GITHUB_API_URL);
  http.addHeader("User-Agent", "CFS-Programmer");

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      String latestVersion = doc["tag_name"].as<String>();
      latestVersion.replace("fw-v", "");
      latestVersion.replace("v", "");

      String downloadURL = doc["assets"][0]["browser_download_url"].as<String>();

      Serial.print("Current version: ");
      Serial.println(FIRMWARE_VERSION);
      Serial.print("Latest version:  ");
      Serial.println(latestVersion);

      if (latestVersion != String(FIRMWARE_VERSION)) {
        Serial.println("🆕 Update available!");
        notifyMac("UPDATE_AVAILABLE:" + latestVersion + "," + downloadURL);
        showMessage("Update found!", latestVersion, "");
      } else {
        Serial.println("✅ Firmware is up to date");
        notifyMac("UP_TO_DATE");
        showMessage("Up to date", FIRMWARE_VERSION, "");
      }
    } else {
      Serial.println("❌ JSON parsing failed");
      notifyMac("ERROR:JSON parse error");
    }
  } else {
    Serial.print("❌ HTTP error: ");
    Serial.println(httpCode);
    notifyMac("ERROR:GitHub API error");
  }

  http.end();
  WiFi.disconnect();

  delay(3000);
  showMessage("Ready!", "Waiting", "");
}

void performOTAUpdate(String firmwareURL) {
  if (!wifiConfigured) {
    notifyMac("ERROR:WiFi not configured");
    return;
  }

  currentState = STATE_UPDATING;

  Serial.println("\n🔄 Starting OTA update...");
  Serial.print("URL: ");
  Serial.println(firmwareURL);

  showMessage("Updating...", "Please wait", "");
  setLED(WS_MAGENTA);

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  HTTPClient http;
  http.begin(firmwareURL);

  int httpCode = http.GET();

  if (httpCode == 200) {
    int contentLength = http.getSize();

    Serial.print("Firmware size: ");
    Serial.print(contentLength);
    Serial.println(" bytes");

    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      WiFiClient *client = http.getStreamPtr();

      size_t written = Update.writeStream(*client);

      if (written == contentLength) {
        Serial.println("✅ Written : " + String(written) + " bytes");
      } else {
        Serial.println("❌ Written only : " + String(written) + "/" + String(contentLength));
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          Serial.println("✅ OTA UPDATE SUCCESSFUL!");
          notifyMac("UPDATE_SUCCESS");
          showMessage("Success!", "Rebooting...", "");
          delay(2000);
          ESP.restart();
        } else {
          Serial.println("❌ Update not finished");
          notifyMac("ERROR:Update incomplete");
        }
      } else {
        Serial.println("❌ Error: " + String(Update.getError()));
        notifyMac("ERROR:Update failed");
      }
    } else {
      Serial.println("❌ Not enough space");
      notifyMac("ERROR:Not enough space");
    }
  } else {
    Serial.println("❌ HTTP error: " + String(httpCode));
    notifyMac("ERROR:Download failed");
  }

  http.end();
  WiFi.disconnect();

  currentState = STATE_IDLE;
  setLED(WS_OFF);
  showMessage("Update failed", "Try again", "");
  delay(3000);
  showMessage("Ready!", "Waiting", "");
}

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   CFS Programmer v" + String(FIRMWARE_VERSION) + "               ║");
  Serial.println("║   ESP32-S3 + PN532                    ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.print("Built: ");
  Serial.print(FIRMWARE_BUILD_DATE);
  Serial.print(" ");
  Serial.println(FIRMWARE_BUILD_TIME);
  Serial.println();

  // LED
  Serial.println("[1/5] Initializing RGB LED...");
  led.begin();
  led.setBrightness(50);
  setLED(WS_BLUE);
  Serial.println("      ✅ LED ready");
  delay(500);
  setLED(WS_OFF);

  // I2C
  Serial.println("[2/5] Initializing I2C bus...");
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Serial.println("      ✅ I2C ready");

  // OLED
  Serial.println("[3/5] Initializing OLED...");
  display.begin();
  Serial.println("      ✅ OLED ready");
  showMessage("CFS Programmer", "v" + String(FIRMWARE_VERSION), "Booting...");

  // PN532
  Serial.println("[4/5] Initializing PN532...");
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("      ❌ PN532 NOT FOUND!");
    setLED(WS_RED);
    showMessage("ERROR!", "PN532 not found", "");
    while(1) delay(1000);
  }

  Serial.print("      ✅ PN532 v");
  Serial.print((versiondata>>24) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata>>16) & 0xFF, DEC);

  nfc.SAMConfig();

  // BLE
  Serial.println("[5/5] Initializing BLE...");
  BLEDevice::init("CFS-Programmer");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  txChar = pService->createCharacteristic(
    TX_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  txChar->addDescriptor(new BLE2902());

  rxChar = pService->createCharacteristic(
    RX_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  rxChar->setCallbacks(new RxCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("      ✅ BLE ready");

  Serial.println();
  Serial.println("🎉 READY!");
  setLED(WS_OFF);
  showMessage("Ready!", "v" + String(FIRMWARE_VERSION), "Waiting...");
}

// ═══════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
  static unsigned long readingStartTime = 0;

  if (currentState == STATE_READING) {
    if (readingStartTime == 0) {
      readingStartTime = millis();
      setLED(WS_BLUE);
      Serial.println("📖 READ mode - place tag");
    }

    if (millis() - readingStartTime > 30000) {
      Serial.println("⏱️  Timeout");
      notifyMac("ERROR:Timeout");
      currentState = STATE_IDLE;
      readingStartTime = 0;
      setLED(WS_OFF);
      showMessage("Timeout", "", "");
      delay(2000);
      showMessage("Ready!", "Waiting", "");
      return;
    }

    if (waitForTag()) {
      readingStartTime = 0;
      setLED(WS_YELLOW);
      showMessage("Reading...", "Please wait", "");

      String result = readTag();

      if (result.startsWith("ERROR:")) {
        setLED(WS_RED);
        notifyMac(result);
        String errMsg = result.substring(6);
        showMessage("Error", errMsg.substring(0, 20), "");
        Serial.print("❌ ");
        Serial.println(result);
        delay(3000);
      } else {
        setLED(WS_GREEN);
        notifyMac("TAG_DATA:" + result);
        showMessage("Success!", "Check Mac app", "");
        Serial.println("✅ Success!");
        delay(2000);
      }

      currentState = STATE_IDLE;
      setLED(WS_OFF);
      showMessage("Ready!", "Waiting", "");
    }
  } else if (currentState == STATE_WRITING) {
    writeTag();
  } else {
    delay(100);
  }
}

// ═══════════════════════════════════════════════════════════
// DISPLAY HELPER
// ═══════════════════════════════════════════════════════════
void showMessage(String line1, String line2, String line3) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 12, line1.c_str());
  display.drawStr(0, 30, line2.c_str());
  display.drawStr(0, 48, line3.c_str());
  display.sendBuffer();
}
