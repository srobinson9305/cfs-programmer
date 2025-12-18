/*
 * CFS Tag Reader - RC522 with THiNX AESLib ECB
 * Reads and parses Creality CFS RFID tags
 */

#include <SPI.h>
#include <MFRC522.h>
#include <AESLib.h>

// Pin definitions for ESP32-S3 VSPI
#define SS_PIN 38
#define RST_PIN 40

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key ekey;
AESLib aesLib;

// AES keys from DnG
// u_key: Used for MIFARE key generation AND data decryption
byte u_key[16] = {113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49};
// d_key: Used for data encryption (when writing tags)
byte d_key[16] = {72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50};

// Zero IV for ECB-like behavior
byte iv[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char tagData[49];

// CFS Tag structure
struct CFSTag {
  String date;          // 5 chars: M DD YY (hex-encoded month)
  String vendorID;      // 4 chars: "0276" = Creality
  String batch;         // 2 chars: Batch number
  String filamentID;    // 6 chars: Material type
  String color;         // 7 chars: Hex color with "0" prefix
  String length;        // 4 chars: Length in meters (hex)
  String serial;        // 6 chars: Serial number
  String reserve;       // 6 chars: Reserved (000000)
  String padding;       // 8 chars: Padding to 48 total
};

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n======================================");
  Serial.println("  CFS TAG READER - Full Parser");
  Serial.println("======================================\n");
  
  SPI.begin(39, 36, 35, 38);
  mfrc522.PCD_Init();
  
  Serial.println("✅ RC522 initialized\n");
  Serial.println("Place a Creality CFS tag on the reader\n");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;
  
  Serial.println("\n========================================");
  Serial.println("✅ TAG DETECTED!");
  Serial.println("========================================");
  
  Serial.print("UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) Serial.print(" ");
  }
  Serial.println();
  
  generateKey();
  
  if (readCFSTag()) {
    Serial.println("\n✅ READ SUCCESS!");
    Serial.println("========================================");
    Serial.print("Raw  ");
    Serial.println(tagData);
    Serial.println("========================================");
    
    // Parse and display the tag data
    CFSTag tag = parseCFSTag(tagData);
    displayTagInfo(tag);
  }
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  delay(3000);
}

void generateKey() {
  byte uid[16];
  byte bufOut[16];
  byte iv_copy[16];
  
  memcpy(iv_copy, iv, 16);
  
  // Repeat UID to fill 16 bytes
  int x = 0;
  for (int i = 0; i < 16; i++) {
    if (x >= 4) x = 0;
    uid[i] = mfrc522.uid.uidByte[x];
    x++;
  }
  
  // Encrypt with u_key (CORRECTED - was using d_key)
  aesLib.encrypt(uid, 16, bufOut, u_key, sizeof(u_key), iv_copy);
  
  // First 6 bytes = MIFARE key
  for (int i = 0; i < 6; i++) {
    ekey.keyByte[i] = bufOut[i];
  }
  
  Serial.print("Generated Key: ");
  for (int i = 0; i < 6; i++) {
    if (ekey.keyByte[i] < 0x10) Serial.print("0");
    Serial.print(ekey.keyByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

bool readCFSTag() {
  Serial.println("\nAuthenticating block 4...");
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &ekey, &(mfrc522.uid));
  
  if (status != MFRC522::STATUS_OK) {
    Serial.print("✗ Auth failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  
  Serial.println("✓ Authenticated!");
  
  // Read blocks 4, 5, 6
  byte encrypted[48];
  byte decrypted[48];
  
  for (byte block = 4; block <= 6; block++) {
    byte buffer[18];
    byte size = sizeof(buffer);
    
    status = mfrc522.MIFARE_Read(block, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("✗ Read block ");
      Serial.print(block);
      Serial.print(" failed");
      return false;
    }
    
    memcpy(&encrypted[(block - 4) * 16], buffer, 16);
  }
  
  // Decrypt with d_key using ECB mode (data is encrypted with d_key)
  Serial.println("Decrypting in ECB mode...");
  
  for (int i = 0; i < 3; i++) {
    byte iv_copy[16];
    byte block_out[16];
    
    memcpy(iv_copy, iv, 16);
    aesLib.decrypt(&encrypted[i * 16], 16, block_out, d_key, sizeof(d_key), iv_copy);
    memcpy(&decrypted[i * 16], block_out, 16);
  }
  
  memcpy(tagData, decrypted, 48);
  tagData[48] = '\0';
  
  return true;
}

CFSTag parseCFSTag(String data) {
  CFSTag tag;
  
  if (data.length() < 40) {
    Serial.println("⚠️  Warning: Tag data too short!");
    return tag;
  }
  
  // Parse according to corrected DnG specification
  tag.date = data.substring(0, 5);          // 0-4: Date (5 chars)
  tag.vendorID = data.substring(5, 9);      // 5-8: Vendor ID (4 chars)
  tag.batch = data.substring(9, 11);        // 9-10: Batch (2 chars)
  tag.filamentID = data.substring(11, 17);  // 11-16: Filament ID (6 chars)
  tag.color = data.substring(17, 24);       // 17-23: Color (7 chars)
  tag.length = data.substring(24, 28);      // 24-27: Length (4 chars)
  tag.serial = data.substring(28, 34);      // 28-33: Serial (6 chars)
  tag.reserve = data.substring(34, 40);     // 34-39: Reserve (6 chars)
  
  if (data.length() >= 48) {
    tag.padding = data.substring(40, 48);   // 40-47: Padding (8 chars)
  }
  
  return tag;
}

void displayTagInfo(CFSTag tag) {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║       CFS TAG INFORMATION              ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  Serial.print("Date:         "); Serial.print(tag.date);
  Serial.print(" ("); Serial.print(decodeDate(tag.date)); Serial.println(")");
  
  Serial.print("Vendor ID:    "); Serial.print(tag.vendorID);
  Serial.print(" ("); Serial.print(decodeVendor(tag.vendorID)); Serial.println(")");
  
  Serial.print("Batch:        "); Serial.println(tag.batch);
  
  Serial.print("Filament ID:  "); Serial.print(tag.filamentID);
  Serial.print(" ("); Serial.print(decodeFilamentID(tag.filamentID)); Serial.println(")");
  
  Serial.print("Color:        "); Serial.print(tag.color);
  Serial.print(" (RGB: #"); Serial.print(tag.color.substring(1)); Serial.println(")");
  
  Serial.print("Length:       "); Serial.print(tag.length);
  Serial.print(" ("); Serial.print(decodeLength(tag.length)); Serial.println(")");
  
  Serial.print("Serial:       "); Serial.println(tag.serial);
  Serial.print("Reserve:      "); Serial.println(tag.reserve);
  
  if (tag.padding.length() > 0) {
    Serial.print("Padding:      "); Serial.println(tag.padding);
  }
  
  Serial.println("════════════════════════════════════════\n");
}

String decodeDate(String dateCode) {
  if (dateCode.length() < 5) return "Invalid";
  
  char monthChar = dateCode.charAt(0);
  int month = 0;
  
  if (monthChar >= 'A' && monthChar <= 'F') {
    month = 10 + (monthChar - 'A');
  } else if (monthChar >= '1' && monthChar <= '9') {
    month = monthChar - '0';
  }
  
  String day = dateCode.substring(1, 3);
  String year = "20" + dateCode.substring(3, 5);
  
  const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  
  if (month >= 1 && month <= 12) {
    return String(months[month]) + " " + day + ", " + year;
  }
  
  return dateCode;
}

String decodeVendor(String vendorID) {
  if (vendorID == "0276") return "Creality";
  return "Unknown (" + vendorID + ")";
}

String decodeFilamentID(String filamentID) {
  if (filamentID == "101001") return "PLA";
  if (filamentID == "101002") return "PETG";
  if (filamentID == "101003") return "ABS";
  if (filamentID == "101004") return "TPU";
  if (filamentID == "101005") return "Nylon";
  return "Unknown (" + filamentID + ")";
}

String decodeLength(String lengthCode) {
  long meters = strtol(lengthCode.c_str(), NULL, 16);
  
  if (meters == 0x0330 || meters == 330) return "330m (1kg)";
  if (meters == 0x0247 || meters == 247) return "247m (750g)";
  if (meters == 0x0198 || meters == 198) return "198m (600g)";
  if (meters == 0x0165 || meters == 165) return "165m (500g)";
  if (meters == 0x0082 || meters == 82) return "82m (250g)";
  
  return String(meters) + "m";
}
