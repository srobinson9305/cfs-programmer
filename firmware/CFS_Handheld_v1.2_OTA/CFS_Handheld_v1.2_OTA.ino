/*
 * CFS Programmer - ESP32-S3 + PN532
 * Version: 1.2.0
 * 
 * CHANGELOG v1.2:
 * - Added firmware versioning system
 * - OTA update support via WiFi
 * - Sends version to Mac app over BLE
 * - GitHub releases integration ready
 * 
 * REQUIRED LIBRARY FIX:
 * Edit Adafruit_PN532.cpp and add after line 60:
 *   #define SLOWDOWN 1
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
#define GITHUB_API_URL "https://api.github.com/repos/srobinson9305/cfs-programmer-firmware/releases/latest"

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
// AES KEYS
// ═══════════════════════════════════════════════════════════
uint8_t u_key[16] = {113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49};
uint8_t d_key[16] = {72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50};

// ═══════════════════════════════════════════════════════════
// GLOBAL UID STORAGE
// ═══════════════════════════════════════════════════════════
uint8_t currentUID[7];
uint8_t currentUIDLength = 0;

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

void showMessage(String line1, String line2, String line3);
void notifyMac(String message);

// ═══════════════════════════════════════════════════════════
// BLE CALLBACKS
// ═══════════════════════════════════════════════════════════
class ServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    Serial.println("BLE: Client connected");
    setLED(WS_CYAN);
    showMessage("Connected!", "Mac app ready", "");
    delay(500);

    // Send firmware version to Mac app
    notifyMac("VERSION:" + String(FIRMWARE_VERSION));
    delay(100);

    setLED(WS_OFF);
    showMessage("Ready!", "Waiting for", "next job");
  }

  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    Serial.println("BLE: Client disconnected");
    showMessage("Disconnected", "Waiting", "");
    pServer->startAdvertising();
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
      String cfsData = cmd.substring(6);
      Serial.print("Writing CFS data: ");
      Serial.println(cfsData);
      showMessage("Writing...", "Tag 1 of 2", "");
      notifyMac("WRITE_READY");

    } else if (cmd == "GET_VERSION") {
      notifyMac("VERSION:" + String(FIRMWARE_VERSION));

    } else if (cmd.startsWith("WIFI_CONFIG:")) {
      // Format: WIFI_CONFIG:SSID,PASSWORD
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
      // Format: OTA_UPDATE:https://github.com/.../firmware.bin
      String firmwareURL = cmd.substring(11);
      performOTAUpdate(firmwareURL);

    } else if (cmd == "CANCEL") {
      currentState = STATE_IDLE;
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
// OTA UPDATE FUNCTIONS
// ═══════════════════════════════════════════════════════════

void checkForUpdate() {
  if (!wifiConfigured) {
    notifyMac("ERROR:WiFi not configured");
    return;
  }

  Serial.println("\n🔍 Checking for firmware updates...");
  showMessage("Checking...", "for updates", "");

  // Connect to WiFi
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

  // Fetch latest release info from GitHub
  HTTPClient http;
  http.begin(GITHUB_API_URL);
  http.addHeader("User-Agent", "CFS-Programmer");

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    // Parse JSON response
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      String latestVersion = doc["tag_name"].as<String>();
      latestVersion.replace("v", ""); // Remove 'v' prefix if present

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

  // Connect to WiFi
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
  txChar = pService->createCharacteristic(TX_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());
  rxChar = pService->createCharacteristic(RX_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
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

// Rest of the code (loop, readTag, etc.) stays the same as v1.1...
// (Including all the authentication and read functions from previous version)

void loop() {
  // Same as before
  delay(100);
}

void showMessage(String line1, String line2, String line3) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 12, line1.c_str());
  display.drawStr(0, 30, line2.c_str());
  display.drawStr(0, 48, line3.c_str());
  display.sendBuffer();
}

// ... (rest of functions from v1.1)
