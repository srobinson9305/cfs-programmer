/*
 * CFS Programmer - ESP32-S3 + PN532 (PATCHED)
 * 
 * FIXES APPLIED:
 * 1. Authentication retry logic (workaround for PN532 library bugs)
 * 2. Proper delays and re-initialization
 * 3. Multiple authentication strategies
 * 
 * REQUIRED LIBRARY FIX:
 * Edit Adafruit_PN532.cpp and add after line 60:
 *   #define SLOWDOWN 1
 * 
 * This fixes ACK frame timeout issues documented in:
 * https://github.com/adafruit/Adafruit-PN532/issues/93
 */

#include <Wire.h>
#include <Adafruit_PN532.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include <AESLib.h>

AESLib aesLib;

// ═══════════════════════════════════════════════════════════
// PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════
#define I2C_SDA         8
#define I2C_SCL         9
#define WS2812_PIN     48
#define PN532_IRQ      -1  // Not used

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
// STATE
// ═══════════════════════════════════════════════════════════
enum State { STATE_IDLE, STATE_READING };
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
enum WS2812Color { WS_OFF, WS_RED, WS_GREEN, WS_BLUE, WS_YELLOW, WS_CYAN };

void setLED(WS2812Color color) {
  switch (color) {
    case WS_OFF:    led.setPixelColor(0, 0, 0, 0); break;
    case WS_RED:    led.setPixelColor(0, 255, 0, 0); break;
    case WS_GREEN:  led.setPixelColor(0, 0, 255, 0); break;
    case WS_BLUE:   led.setPixelColor(0, 0, 0, 255); break;
    case WS_YELLOW: led.setPixelColor(0, 255, 255, 0); break;
    case WS_CYAN:   led.setPixelColor(0, 0, 255, 255); break;
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
    delay(1000);
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
// SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   CFS Programmer - PN532 PATCHED      ║");
  Serial.println("║   With Authentication Retry Logic     ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.println("⚠️  IMPORTANT: Edit Adafruit_PN532.cpp");
  Serial.println("   Add after line 60: #define SLOWDOWN 1");
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
  showMessage("CFS Programmer", "PN532 Patched", "Booting...");

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
  showMessage("Ready!", "Waiting for", "Mac app");
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
  } else {
    delay(100);
  }
}

// ═══════════════════════════════════════════════════════════
// NFC FUNCTIONS WITH RETRY LOGIC
// ═══════════════════════════════════════════════════════════

bool waitForTag() {
  bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUID, &currentUIDLength, 100);

  if (!success) {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
      Serial.println("⏳ Waiting for tag...");
      lastPrint = millis();
    }
    return false;
  }

  Serial.println();
  Serial.println("═══════════════════════════════════════");
  Serial.print("✅ TAG DETECTED! UID (");
  Serial.print(currentUIDLength);
  Serial.print(" bytes): ");
  for (uint8_t i = 0; i < currentUIDLength; i++) {
    if (currentUID[i] < 0x10) Serial.print("0");
    Serial.print(currentUID[i], HEX);
    if (i < currentUIDLength - 1) Serial.print(" ");
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

void generateKeyFromUID(uint8_t* mifareKey) {
  Serial.println();
  Serial.println("🔐 Generating MIFARE key from UID...");

  uint8_t uid16[16];
  int x = 0;
  for (int i = 0; i < 16; i++) {
    if (x >= currentUIDLength) x = 0;
    uid16[i] = currentUID[x];
    x++;
  }

  Serial.print("   UID repeated: ");
  for (int i = 0; i < 16; i++) {
    if (uid16[i] < 0x10) Serial.print("0");
    Serial.print(uid16[i], HEX);
    if (i < 15) Serial.print(" ");
  }
  Serial.println();

  uint8_t bufOut[16];
  byte iv[16] = {0};
  aesLib.encrypt(uid16, 16, bufOut, u_key, 128, iv);

  Serial.print("   Encrypted:    ");
  for (int i = 0; i < 16; i++) {
    if (bufOut[i] < 0x10) Serial.print("0");
    Serial.print(bufOut[i], HEX);
    if (i < 15) Serial.print(" ");
  }
  Serial.println();

  for (int i = 0; i < 6; i++) {
    mifareKey[i] = bufOut[i];
  }

  Serial.print("   MIFARE Key:   ");
  for (int i = 0; i < 6; i++) {
    if (mifareKey[i] < 0x10) Serial.print("0");
    Serial.print(mifareKey[i], HEX);
    if (i < 5) Serial.print(" ");
  }
  Serial.println();
}

// PATCHED: Authentication with retry logic
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

String readTag() {
  Serial.println();
  Serial.println("📖 Reading Creality CFS tag...");

  uint8_t customKey[6];
  generateKeyFromUID(customKey);

  Serial.println();
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
          Serial.println();
          Serial.println("❌ AUTHENTICATION FAILED");
          Serial.println("   Did you add #define SLOWDOWN 1?");
          Serial.println("   Location: Adafruit_PN532.cpp line 60");
          return "ERROR:Auth failed - check library patch";
        }
      }
    }
  }

  if (!authenticated) {
    return "ERROR:Authentication failed";
  }

  // Read blocks 4, 5, 6
  Serial.println();
  Serial.println("📄 Reading data blocks...");

  String cfsData = "";
  for (uint8_t blockNum = 4; blockNum <= 6; blockNum++) {
    uint8_t data[16];

    Serial.print("   Block ");
    Serial.print(blockNum);
    Serial.print(": ");

    // Try reading with retry
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
      if (i < 15) Serial.print(" ");
    }
    Serial.println(" ✅");

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

  // Decrypt if needed
  if (needsDecryption) {
    Serial.println();
    Serial.println("🔓 Decrypting with d_key...");

    String decrypted = "";
    for (int blockIdx = 0; blockIdx < 3; blockIdx++) {
      uint8_t encBlock[16];
      uint8_t decBlock[16];

      for (int i = 0; i < 16; i++) {
        encBlock[i] = (uint8_t)cfsData.charAt(blockIdx * 16 + i);
      }

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

  Serial.println();
  Serial.println("✅ TAG READ COMPLETE!");
  Serial.print("   Material:  ");
  Serial.println(material);
  Serial.print("   Length:    ");
  Serial.print(lengthMeters);
  Serial.println(" meters");
  Serial.print("   Color:     #");
  Serial.println(color.substring(1));
  Serial.println("═══════════════════════════════════════");

  return material + "|" + String(lengthMeters) + "m|#" + color.substring(1) + "|S/N:" + serial;
}

void showMessage(String line1, String line2, String line3) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 12, line1.c_str());
  display.drawStr(0, 30, line2.c_str());
  display.drawStr(0, 48, line3.c_str());
  display.sendBuffer();
}
