# CFS RFID Tag Format - Complete Reference
## Extracted from DnG K2-RFID Project

---

## **1. AES Encryption Keys**

The DnG project uses **two AES-128 keys** for encryption:

### **User Key (u_key)** - For generating MIFARE keys from UID
```cpp
static byte u_key[16] = {
  113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49
};
```

**Hex representation:**
```
71 33 62 75 5E 74 31 6E 71 66 5A 28 70 66 24 31
```

### **Data Key (d_key)** - For encrypting tag data
```cpp
static byte d_key[16] = {
  72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50
};
```

**Hex representation:**
```
48 40 43 46 6B 52 6E 7A 40 4B 41 74 42 4A 70 32
```

**Note:** In the AES.cpp code, keytype mapping is:
- keytype=0 → uses u_key
- keytype=1 → uses d_key

---

## **2. MIFARE Key Generation from UID**

Creality tags use **UID-derived keys** for authentication.

### **Algorithm (from createKey() function):**

```cpp
void createKey() {
  int x = 0;
  byte uid[16];
  byte bufOut[16];
  
  // Fill 16-byte buffer by repeating the 4-byte UID
  for (int i = 0; i < 16; i++) {
    if (x >= 4) x = 0;
    uid[i] = mfrc522.uid.uidByte[x];
    x++;
  }
  
  // Encrypt the repeated UID with d_key (keytype=0)
  aes.encrypt(0, uid, bufOut);
  
  // Use first 6 bytes as MIFARE Key A
  for (int i = 0; i < 6; i++) {
    ekey.keyByte[i] = bufOut[i];
  }
}
```

### **Process:**
1. Take the 4-byte UID (e.g., `04 A1 B2 C3`)
2. Repeat it 4 times to make 16 bytes: `04 A1 B2 C3 04 A1 B2 C3 04 A1 B2 C3 04 A1 B2 C3`
3. Encrypt this with AES using **d_key**
4. Take the first 6 bytes of the encrypted result as MIFARE Key A
5. This key is used to authenticate and write to the tag

---

## **3. CFS Tag Data Format**

### **Complete 48-Character String Structure:**

```
Position: 00-01  02-06  07-10  11-12  13-18    19-25      26-29    30-35      36-41    42-47
Field:    AB     124    0276   A2     filamentId  color   length   serial     reserve  padding
Example:  AB     124    0276   A2     101001   00FFFF     0165     000001     000000   00000000
```

### **Field Breakdown:**

| Field | Position | Length | Description | Example |
|-------|----------|--------|-------------|---------|
| **Prefix** | 0-1 | 2 chars | Always "AB" | AB |
| **Date** | 2-6 | 5 chars | Date code (M DD YY) | 124 = January 24, 2024 |
| **Vendor ID** | 7-10 | 4 chars | Manufacturer code | 0276 = Creality |
| **Batch** | 11-12 | 2 chars | Batch number | A2 |
| **Filament ID** | 13-18 | 6 chars | Material type from database | 101001 = PLA |
| **Color** | 19-25 | 7 chars | Hex color with "0" prefix | 00FFFF = White |
| **Length** | 26-29 | 4 chars | Filament length in meters | 0165 = 165m (500g) |
| **Serial** | 30-35 | 6 chars | Unique serial number | 000001 |
| **Reserve** | 36-41 | 6 chars | Reserved (always 000000) | 000000 |
| **Padding** | 42-47 | 8 chars | Padding (always 00000000) | 00000000 |

### **Filament Length Values:**
```cpp
1 KG   = 0330  (330 meters)
750 G  = 0247  (247 meters)
600 G  = 0198  (198 meters)
500 G  = 0165  (165 meters)
250 G  = 0082  (82 meters)
```

### **Example Tag Data:**
```
AB1240276A21010010FFFFFF0165000001000000000000
```
**Decoded:**
- Date: 124 (January 24, 2024)
- Vendor: 0276 (Creality)
- Batch: A2
- Material: 101001 (PLA)
- Color: 0FFFFFF (White)
- Length: 0165 (165m / 500g)
- Serial: 000001
- Reserve: 000000
- Padding: 00000000

---

## **4. Tag Writing Process**

### **MIFARE Classic 1K Memory Layout:**

- **Block 0:** Manufacturer data (read-only)
- **Blocks 1-2:** Available for user data
- **Block 3:** Sector 0 trailer (keys + access bits)
- **Blocks 4-6:** CFS data (encrypted)
- **Block 7:** Sector 1 trailer (contains custom keys)
- **Blocks 8-63:** Available

### **CFS Data Storage (DnG Method):**

**Block 4-6:** Contains the 48-character CFS string split into 3 blocks of 16 bytes each:
- Block 4: Characters 0-15 (encrypted)
- Block 5: Characters 16-31 (encrypted)
- Block 6: Characters 32-47 (encrypted)

### **Encryption Process:**
```cpp
byte blockData[17];
byte encData[16];
int blockID = 4;

for (int i = 0; i < spoolData.length(); i += 16) {
  spoolData.substring(i, i + 16).getBytes(blockData, 17);
  if (blockID >= 4 && blockID < 7) {
    aes.encrypt(1, blockData, encData);  // Uses u_key (keytype=1)
    mfrc522.MIFARE_Write(blockID, encData, 16);
  }
  blockID++;
}
```

**Steps:**
1. Split 48-char string into 3 chunks of 16 characters
2. Convert each chunk to bytes
3. Encrypt with AES using **u_key** (keytype=1)
4. Write encrypted data to blocks 4, 5, 6

### **Sector Trailer Update (Block 7):**
After writing data, the sector trailer is updated with the custom key:
```cpp
if (!encrypted) {
  byte buffer[18];
  byte byteCount = sizeof(buffer);
  byte block = 7;
  status = mfrc522.MIFARE_Read(block, buffer, &byteCount);
  
  // Write Key B (bytes 10-15)
  int y = 0;
  for (int i = 10; i < 16; i++) {
    buffer[i] = ekey.keyByte[y];
    y++;
  }
  
  // Write Key A (bytes 0-5)
  for (int i = 0; i < 6; i++) {
    buffer[i] = ekey.keyByte[i];
  }
  
  mfrc522.MIFARE_Write(7, buffer, 16);
}
```

---

## **5. Reading CFS Tags**

### **Authentication Process:**

1. **Try default key first:**
   ```cpp
   key = {255, 255, 255, 255, 255, 255};  // 0xFF x 6
   status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &uid);
   ```

2. **If default fails, generate custom key from UID:**
   ```cpp
   createKey();  // Generate ekey from UID
   status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &ekey, &uid);
   ```

3. **Read and decrypt blocks 4-6:**
   ```cpp
   // Read encrypted data
   byte encData[16];
   mfrc522.MIFARE_Read(4, encData, &size);
   
   // Decrypt with u_key
   byte plainData[16];
   aes.decrypt(1, encData, plainData);
   
   // Convert to string
   String tagData = String((char*)plainData);
   ```

---

## **6. Key Findings for Our Project**

### **Critical Information:**
1. ✅ We now have both AES keys (u_key and d_key)
2. ✅ We understand the UID-to-MIFARE-key algorithm
3. ✅ We know the exact CFS data format (48 chars)
4. ✅ We know which blocks to write (4, 5, 6)
5. ✅ We understand the encryption process

### **What This Means:**
- We can **read** any Creality CFS tag (with custom keys)
- We can **write** new tags that the K2 Pro will recognize
- We can **duplicate** existing tags (same serial on front/back)
- We can **create custom filament profiles**

### **Implementation Path:**
1. Port the AES code to our ESP32-S3
2. Adapt the PN532 library to use the custom keys
3. Build UI for selecting filament type
4. Generate proper CFS strings
5. Write and verify tags
6. Test with K2 Pro printer

---

## **7. Next Steps**

### **Immediate Tasks:**
1. ✅ Create test sketch with AES encryption
2. ✅ Read a real Creality tag and decode it
3. ✅ Write a test tag with known data
4. ✅ Verify the K2 Pro recognizes our tag
5. Build the full dual-tag write workflow

### **Code to Port:**
- `AES.cpp/h` - AES encryption engine
- `createKey()` - UID-to-MIFARE-key generation
- Tag formatting functions
- Block read/write with encryption

---

## **References**

- DnG K2-RFID GitHub: https://github.com/DnG-Crafts/K2-RFID
- MIFARE Classic 1K datasheet
- AES-128 standard
- Creality K2 Pro documentation

---

*Document created from DnG ESP32 project analysis*
*All credit to DnG-Crafts for reverse engineering the CFS format*
