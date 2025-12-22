# Creality CFS Tag Format Documentation

## Overview

Creality CFS (Creality Filament System) tags are MIFARE Classic 1K NFC tags that store filament information in an encrypted format. Each spool comes with two tags (front and back) containing identical data.

## MIFARE Classic 1K Structure

### Memory Layout

Total: 1024 bytes = 16 sectors × 4 blocks × 16 bytes

```
Sector 0 (Manufacturer Block - READ ONLY)
  Block 0: [UID + BCC + Manufacturer Data]
  Block 1: User data
  Block 2: User data
  Block 3: Sector Trailer (Key A, Access Bits, Key B)

Sectors 1-15 (User Sectors)
  Block 0-2: User data (16 bytes each)
  Block 3: Sector Trailer

CFS data is stored in Sector 1, Blocks 4-6
```

### Block Layout

```
Block 0: UID + Manufacturer (READ ONLY)
Block 1: Empty / Not used by CFS
Block 2: Empty / Not used by CFS
Block 3: Sector 0 Trailer

Block 4: CFS Data (bytes 0-15)   ← ENCRYPTED
Block 5: CFS Data (bytes 16-31)  ← ENCRYPTED
Block 6: CFS Data (bytes 32-47)  ← ENCRYPTED
Block 7: Sector 1 Trailer (Keys + Access Bits)
```

## CFS Data Format (48 bytes)

### Raw Format (Before Encryption)

```
Bytes 0-47 (48 total bytes):

[Date][Vendor][??][Film ID][Color ][Length][Serial][Reserved    ]
  5     4      2     6        7      4      6       14

Position breakdown:
00-04: Date code (5 bytes ASCII)
05-08: Vendor code (4 bytes ASCII) 
09-10: Unknown/Batch (2 bytes ASCII)
11-16: Filament type ID (6 bytes ASCII)
17-23: Color code (7 bytes ASCII)
24-27: Length in meters (4 bytes hex)
28-33: Serial number (6 bytes ASCII)
34-47: Reserved/padding (14 bytes, usually 0x00)
```

### Example Real Tag Data

**Decrypted CFS data:**
```
AB C2112 0276 01 101001 0FF5F0B 0165 736314 00000000000000
│  │     │    │  │      │       │    │      └─ Reserved (14 bytes)
│  │     │    │  │      │       │    └─ Serial: 736314
│  │     │    │  │      │       └─ Length: 0165 hex = 357 dec = 357m
│  │     │    │  │      └─ Color: 0FF5F0B (leading 0, then #FF5F0B)
│  │     │    │  └─ Film ID: 101001 (PLA)
│  │     │    └─ Batch/Unknown: 01
│  │     └─ Vendor: 0276 (Creality)
│  └─ Date: C2112 (December 21, 2012 or custom encoding)
└─ Header: AB (constant)
```

## Field Specifications

### Date Code (5 bytes)

Format: `ABXYZ` where:
- `AB` = Constant header (always "AB")
- `X` = Month in hex (C = 12 = December)
- `YZ` = Day + Year encoded

**Note:** Date encoding is not fully reverse-engineered. May be decorative or internal use only.

### Vendor Code (4 bytes)

Known vendors:
- `0276` = Creality (most common)
- `0000` = Generic/Unknown

### Batch/Unknown (2 bytes)

Purpose unclear. Observed values:
- `01`, `02`, `A2` = Different production batches?
- Does not affect printer operation

### Filament Type ID (6 bytes)

Standard Creality IDs:

| ID | Material | Description |
|----|----------|-------------|
| `101001` | PLA | Standard PLA |
| `101002` | PETG | PETG |
| `101003` | ABS | ABS |
| `101004` | TPU | Flexible TPU |
| `101005` | Nylon | PA Nylon |
| `E00003` | PLA | PLA (alternate ID) |

**Note:** The printer uses this ID to set print temperature and speed profiles.

### Color Code (7 bytes)

Format: `0RRGGBB` (leading zero + 6-digit hex color)

Examples:
- `0FFFFFF` = White (#FFFFFF)
- `0FF0000` = Red (#FF0000)
- `000FF00` = Green (#00FF00)
- `0000000` = Black (#000000)
- `0FF5F0B` = Orange (#FF5F0B)
- `09CFF4F` = Lime green (#9CFF4F)

**Why leading zero?** Ensures color code is exactly 7 bytes for consistent parsing.

### Length (4 bytes hex)

Filament length in meters, encoded as 4-digit hex string.

| Hex | Decimal | Weight | Typical Use |
|-----|---------|--------|-------------|
| `0082` | 130 | 250g | Sample spools |
| `0165` | 357 | 500g | Small spools |
| `0198` | 408 | 600g | Medium spools |
| `0247` | 583 | 750g | Large spools |
| `0330` | 816 | 1kg | Standard spools |

**Calculation:** For PLA (density ~1.24 g/cm³, 1.75mm diameter):
- 1kg = ~330m
- 500g = ~165m

### Serial Number (6 bytes)

Unique identifier, appears to be sequential or random 6-digit number.

Examples: `736314`, `000001`, `123456`

**Purpose:**
- Quality control tracking
- Warranty verification
- Batch tracing
- Both tags on same spool share same serial

### Reserved (14 bytes)

All observed tags have zeros here: `00000000000000`

May be used for:
- Future features
- Padding to fill 48-byte block
- Checksum (unused)

## Encryption

### AES-128 CBC Mode

CFS uses AES-128 encryption in CBC (Cipher Block Chaining) mode.

**Encryption Keys (hardcoded):**

```cpp
// Encryption key (u_key) - for UID-based MIFARE key
uint8_t u_key[16] = {
  113, 51, 98, 117, 94, 116, 49, 110,
  113, 102, 90, 40, 112, 102, 36, 49
};

// Decryption key (d_key) - for CFS data
uint8_t d_key[16] = {
  72, 64, 67, 70, 107, 82, 110, 122,
  64, 75, 65, 116, 66, 74, 112, 50
};
```

**Initialization Vector (IV):**
```cpp
byte iv[16] = {0}; // All zeros
```

### Encryption Process

1. **Prepare plaintext CFS data** (48 bytes)
   ```
   ABC21120276A2101001... (ASCII string)
   ```

2. **Encrypt in 16-byte blocks** using d_key
   ```cpp
   for (int block = 0; block < 3; block++) {
     uint8_t plainBlock[16];  // 16 bytes from CFS data
     uint8_t encBlock[16];    // Output buffer

     aesLib.encrypt(plainBlock, 16, encBlock, d_key, 128, iv);

     // Write encBlock to MIFARE block (4, 5, or 6)
   }
   ```

3. **Write to tag blocks 4, 5, 6**

### Decryption Process

1. **Read blocks 4, 5, 6** from tag (48 bytes total)

2. **Decrypt in 16-byte blocks** using d_key
   ```cpp
   for (int block = 0; block < 3; block++) {
     uint8_t encBlock[16];   // Read from tag
     uint8_t plainBlock[16]; // Output buffer

     aesLib.decrypt(encBlock, 16, plainBlock, d_key, 128, iv);

     // Append to result string
   }
   ```

3. **Parse decrypted ASCII data**

## MIFARE Authentication

### UID-Based Key Generation

Creality uses the tag's UID to generate a custom MIFARE authentication key.

**Algorithm:**

1. **Repeat UID to fill 16 bytes**
   ```cpp
   // Example UID: 60 EA 12 21 (4 bytes)
   uint8_t uid16[16];
   int x = 0;
   for (int i = 0; i < 16; i++) {
     uid16[i] = uid[x];
     x++;
     if (x >= uidLength) x = 0;
   }
   // Result: 60 EA 12 21 60 EA 12 21 60 EA 12 21 60 EA 12 21
   ```

2. **Encrypt with u_key**
   ```cpp
   uint8_t encrypted[16];
   byte iv[16] = {0};
   aesLib.encrypt(uid16, 16, encrypted, u_key, 128, iv);
   ```

3. **Take first 6 bytes as MIFARE Key A**
   ```cpp
   uint8_t mifareKey[6];
   for (int i = 0; i < 6; i++) {
     mifareKey[i] = encrypted[i];
   }
   // Example: 1F 1E 83 A9 71 82
   ```

### Authentication Sequence

For a tag with UID `60 EA 12 21`:

1. Generate Key A: `1F 1E 83 A9 71 82`
2. Authenticate to sector 1 block 7 (sector trailer)
3. If successful, can read/write blocks 4, 5, 6

**Retry Logic (Important!):**

The PN532 sometimes fails authentication due to timing. Always retry:

```cpp
bool authenticateWithRetry(uint8_t block) {
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (attempt > 1) {
      // Re-select tag
      nfc.readPassiveTargetID(...);
      delay(100);
    }

    if (nfc.mifareclassic_AuthenticateBlock(...)) {
      return true; // Success!
    }
  }
  return false; // Failed after 3 attempts
}
```

### Sector Trailer Format

Block 7 (Sector 1 Trailer):

```
[Key A  ][Access Bits][Key B  ]
 6 bytes   4 bytes     6 bytes
```

**Default Keys:**
- Key A: `FF FF FF FF FF FF` (for blank tags)
- Key B: `FF FF FF FF FF FF`

**After Writing:**
- Key A: UID-derived (e.g., `1F 1E 83 A9 71 82`)
- Access Bits: Control read/write permissions
- Key B: Usually same as Key A or default

## Complete Read/Write Workflow

### Reading a Tag

```
1. Wait for tag (PN532 readPassiveTargetID)
2. Get UID (e.g., 60 EA 12 21)
3. Generate MIFARE Key A from UID
4. Try authentication with:
   a. Default Key (FF×6)
   b. UID-derived Key A
   c. UID-derived Key B
   d. Try block 4 directly
5. If authenticated, read blocks 4, 5, 6
6. Check if all zeros (blank tag)
7. If not blank, decrypt with d_key
8. Parse ASCII string into fields
9. Return: Material, Length, Color, Serial
```

### Writing a Tag (Dual-Tag Mode)

```
1. Generate CFS data string (48 bytes ASCII)
2. Encrypt with d_key → 48 bytes encrypted
3. Wait for Tag 1
4. Authenticate (may use default key for blank tag)
5. Write blocks 4, 5, 6
6. Update sector trailer with UID-derived keys
7. Verify write
8. Wait for Tag 2
9. Repeat steps 4-7
10. Both tags now have identical data
```

## Blank Tag Detection

A tag is considered "blank" if all 48 bytes in blocks 4-6 are `0x00`.

```cpp
bool isBlank = true;
for (int i = 0; i < 48; i++) {
  if (data[i] != 0x00) {
    isBlank = false;
    break;
  }
}
```

Blank tags use default keys (`FF×6`) and can be written immediately.

## Security Considerations

### Key Hardcoding

The AES keys (`u_key` and `d_key`) are **hardcoded** in:
- Creality printer firmware
- This CFS Programmer
- Any tool that reads CFS tags

**Implication:** Anyone with these keys can read/write tags. This is intentional for ecosystem compatibility.

### UID-Based Keys Weakness

Using UID to generate MIFARE keys provides minimal security:
- UIDs are readable without authentication
- Algorithm is deterministic
- Once you know the algorithm, any tag can be unlocked

**Implication:** Tags are not cryptographically secure, but prevent casual modification.

### Cloning Tags

With this programmer, you can:
1. Read original tag
2. Write data to blank MIFARE Classic 1K tags
3. Create identical pairs

**Legal note:** Clone only your own tags for backup purposes.

## Compatibility

### Printers

Known compatible Creality printers:
- K2 Plus
- K2 Pro (primary target)
- K1 series (may require testing)
- CR-Series with CFS retrofit

### Tags

Compatible MIFARE tags:
- **MIFARE Classic 1K** (NXP)
- **MIFARE Classic 1K S50** (Chinese clones)
- **FM11RF08** (compatible IC)

**NOT compatible:**
- MIFARE Ultralight (too small)
- NTAG215/216 (different protocol)
- MIFARE DESFire (different encryption)

## Troubleshooting

### "Not a Creality CFS Tag" Error

**Causes:**
1. Wrong tag type (not MIFARE Classic 1K)
2. Corrupted data in blocks 4-6
3. Encrypted with different keys
4. Damaged tag

**Solution:** Verify tag type with NFC Tools app.

### Authentication Fails

**Causes:**
1. Missing `SLOWDOWN 1` patch in library
2. Poor NFC connection
3. Previously written with different keys
4. Tag is locked

**Solution:** 
1. Apply library patch
2. Hold tag steady on reader
3. Try blank tag

### Decrypted Data is Garbage

**Causes:**
1. Wrong d_key
2. Tag not encrypted (already plain text)
3. Non-CFS tag

**Solution:** Check if data looks like ASCII before decrypting.

## Future Enhancements

Possible additions to format:
- **Bytes 34-35:** Checksum (CRC16 of bytes 0-33)
- **Bytes 36-39:** Manufacturing date (Unix timestamp)
- **Bytes 40-41:** Recommended nozzle temp
- **Bytes 42-43:** Recommended bed temp
- **Bytes 44-47:** Reserved for future use

These would require firmware updates on both programmer and printer.
