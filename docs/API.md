# CFS Programmer API Documentation

## BLE Protocol Specification

Version 1.2.0

## Overview

The CFS Programmer communicates with the Mac app via Bluetooth Low Energy (BLE) using a simple text-based protocol. Commands are sent from the Mac app to the device (RX characteristic), and responses are sent from the device to the Mac app (TX characteristic via notifications).

## BLE Service

### Service UUID
```
4fafc201-1fb5-459e-8fcc-c5c9c331914b
```

### Characteristics

#### TX Characteristic (Device â†’ Mac)
- **UUID:** `1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e`
- **Properties:** Notify
- **Purpose:** Device sends responses and status updates
- **Format:** UTF-8 text strings

#### RX Characteristic (Mac â†’ Device)
- **UUID:** `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- **Properties:** Write
- **Purpose:** Mac app sends commands
- **Format:** UTF-8 text strings

## Command Reference

All commands and responses are case-sensitive ASCII strings.

### Connection and Version

#### Get Firmware Version

**Command:**
```
GET_VERSION
```

**Response:**
```
VERSION:1.2.0
```

**Notes:**
- Version is also sent automatically when Mac app first connects
- Format: `MAJOR.MINOR.PATCH` (Semantic Versioning)

---

### Tag Operations

#### Read Tag

**Command:**
```
READ
```

**Immediate Response:**
```
READY
```

**Flow:**
1. Mac sends: `READ`
2. Device responds: `READY`
3. User places tag on reader
4. Device detects tag: `UID:60ea1221` (4-8 hex bytes, lowercase)
5. Device reads and processes
6. Device sends one of:
   - `TAG_DATA:PLA|330m|#FFFFFF|S/N:123456` (success)
   - `BLANK_TAG` (tag is blank/empty)
   - `ERROR:<message>` (failure)

**TAG_DATA Format:**
```
TAG_DATA:<material>|<length>|#<color>|S/N:<serial>
```

Example:
```
TAG_DATA:PLA|330m|#9CFF4F|S/N:736314
```

**Timeout:**
- 30 seconds if no tag is presented
- Response: `ERROR:Timeout`

---

#### Write Dual Tag Set

**Command:**
```
WRITE:<cfs_data>
```

Where `<cfs_data>` is 48 characters of CFS data in ASCII hex format.

**Example:**
```
WRITE:ABC21120276A21010010FF5F0B0330736314000000000000000
```

**Format Breakdown:**
```
AB      = Header (constant)
C2112   = Date code
0276    = Vendor (Creality)
A2      = Unknown/Batch
101001  = Film ID (PLA)
0FF5F0B = Color (#FF5F0B with leading 0)
0330    = Length (816m in hex)
736314  = Serial number
00000000000000 = Reserved (14 bytes)
```

**Flow:**
1. Mac sends: `WRITE:<cfs_data>`
2. Device responds: `WRITE_READY`
3. Device waits for Tag 1
4. User places Tag 1 â†’ Device writes â†’ `TAG1_WRITTEN`
5. Device waits for Tag 2
6. User places Tag 2 â†’ Device writes â†’ `TAG2_WRITTEN`
7. Complete! Both tags have identical data

**Timeout:**
- 60 seconds per tag
- Response: `ERROR:Timeout`

---

#### Cancel Operation

**Command:**
```
CANCEL
```

**Response:**
None (device returns to idle)

**Purpose:**
- Abort read operation
- Abort write operation
- Clear error state

---

### WiFi and OTA Updates

#### Configure WiFi

**Command:**
```
WIFI_CONFIG:<ssid>,<password>
```

**Example:**
```
WIFI_CONFIG:MyNetwork,MyPassword123
```

**Response:**
```
WIFI_OK
```

**Notes:**
- Credentials are stored in device RAM (not persistent)
- Required before OTA updates
- Device does NOT connect to WiFi immediately
- WiFi is only used when checking/installing updates

---

#### Check for Firmware Update

**Command:**
```
CHECK_UPDATE
```

**Flow:**
1. Mac sends: `CHECK_UPDATE`
2. Device connects to WiFi
3. Device fetches: `https://api.github.com/repos/USERNAME/REPO/releases/latest`
4. Device compares versions
5. Device responds with one of:
   - `UPDATE_AVAILABLE:1.3.0,https://github.com/.../firmware.bin`
   - `UP_TO_DATE`
   - `ERROR:WiFi not configured`
   - `ERROR:WiFi connection failed`
   - `ERROR:GitHub API error`

**UPDATE_AVAILABLE Format:**
```
UPDATE_AVAILABLE:<version>,<download_url>
```

Example:
```
UPDATE_AVAILABLE:1.3.0,https://github.com/user/repo/releases/download/fw-v1.3.0/firmware.bin
```

**Timeout:**
- 30 seconds for WiFi connection
- 10 seconds for HTTP request

---

#### Perform OTA Update

**Command:**
```
OTA_UPDATE:<firmware_url>
```

**Example:**
```
OTA_UPDATE:https://github.com/user/repo/releases/download/fw-v1.3.0/firmware.bin
```

**Flow:**
1. Mac sends: `OTA_UPDATE:<url>`
2. Device connects to WiFi
3. Device downloads .bin file (30-60 seconds)
4. Device flashes firmware to OTA partition
5. Device sends: `UPDATE_SUCCESS`
6. Device reboots
7. New firmware starts
8. Device sends: `VERSION:1.3.0`

**Possible Responses:**
- `UPDATE_SUCCESS` (before reboot)
- `ERROR:WiFi not configured`
- `ERROR:Download failed`
- `ERROR:Not enough space`
- `ERROR:Update failed`

**Update Time:**
- ~30-60 seconds typical
- Device will disconnect/reconnect during reboot

---

## Response Messages

### Success Responses

| Response | Meaning |
|----------|---------|
| `READY` | Ready for tag operation |
| `WRITE_READY` | Ready to write tags |
| `TAG1_WRITTEN` | First tag written successfully |
| `TAG2_WRITTEN` | Second tag written successfully |
| `WIFI_OK` | WiFi credentials saved |
| `UP_TO_DATE` | Firmware is current |
| `UPDATE_SUCCESS` | OTA update completed |

### Data Responses

| Response | Format | Example |
|----------|--------|---------|
| `VERSION` | `VERSION:<version>` | `VERSION:1.2.0` |
| `UID` | `UID:<hex>` | `UID:60ea1221` |
| `TAG_DATA` | `TAG_DATA:<mat>\|<len>\|#<col>\|S/N:<ser>` | `TAG_DATA:PLA\|330m\|#FFFFFF\|S/N:123456` |
| `BLANK_TAG` | `BLANK_TAG` | Tag is empty/blank |
| `UPDATE_AVAILABLE` | `UPDATE_AVAILABLE:<ver>,<url>` | See above |

### Error Responses

All errors start with `ERROR:` followed by description.

| Error | Cause |
|-------|-------|
| `ERROR:Auth failed - check library patch` | PN532 authentication timeout |
| `ERROR:Not a Creality CFS Tag` | Wrong tag type or corrupted data |
| `ERROR:Read failed on block X` | Failed to read MIFARE block |
| `ERROR:Timeout` | No tag presented within timeout |
| `ERROR:WiFi not configured` | Need to send WIFI_CONFIG first |
| `ERROR:WiFi connection failed` | Can't connect to network |
| `ERROR:GitHub API error` | HTTP request failed |
| `ERROR:Download failed` | OTA download error |
| `ERROR:Not enough space` | Insufficient flash for update |
| `ERROR:Update failed` | OTA flash operation failed |

---

## State Machine

The device operates in one of 4 states:

```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ STATE_IDLE  â”‚ â†â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â””â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”˜                   â”‚
       â”‚                          â”‚
  READ â”‚                          â”‚
       â†“                          â”‚
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”               â”‚
â”‚ STATE_READING   â”‚               â”‚
â”‚ (wait for tag)  â”‚               â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”˜               â”‚
         â”‚                        â”‚
    Tag detected                  â”‚
         â”‚                        â”‚
    Read & Parse â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
         â”‚
         â”‚
         â”‚
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ STATE_IDLE  â”‚
â””â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”˜
       â”‚
 WRITE â”‚
       â†“
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ STATE_WRITING   â”‚
â”‚ (Tag 1 of 2)    â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”˜
         â”‚
    Tag 1 written
         â”‚
         â†“
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ STATE_WRITING   â”‚
â”‚ (Tag 2 of 2)    â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”˜
         â”‚
    Tag 2 written â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
         â”‚                         â”‚
         â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜

â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ STATE_IDLE  â”‚
â””â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”˜
       â”‚
OTA_UPDATE
       â†“
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ STATE_UPDATING  â”‚
â”‚ (downloading)   â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”˜
         â”‚
    Flash & Reboot
         â”‚
         â†“
   [Device Restarts]
```

---

## Example Communication Flows

### Example 1: Read a Tag

```
Mac â†’ Device:  READ
Device â†’ Mac:  READY
[User places tag]
Device â†’ Mac:  UID:60ea1221
Device â†’ Mac:  TAG_DATA:PLA|330m|#FFFFFF|S/N:123456
```

### Example 2: Read a Blank Tag

```
Mac â†’ Device:  READ
Device â†’ Mac:  READY
[User places tag]
Device â†’ Mac:  UID:4abffc03
Device â†’ Mac:  BLANK_TAG
```

### Example 3: Write Dual Tags

```
Mac â†’ Device:  WRITE:ABC21120276A21010010FFFFFF0330123456000000000000000
Device â†’ Mac:  WRITE_READY
[User places Tag 1]
Device â†’ Mac:  TAG1_WRITTEN
[User places Tag 2]
Device â†’ Mac:  TAG2_WRITTEN
```

### Example 4: OTA Update

```
Mac â†’ Device:  WIFI_CONFIG:MyWiFi,MyPassword
Device â†’ Mac:  WIFI_OK

Mac â†’ Device:  CHECK_UPDATE
[Device connects to WiFi and checks GitHub]
Device â†’ Mac:  UPDATE_AVAILABLE:1.3.0,https://github.com/.../fw.bin

Mac â†’ Device:  OTA_UPDATE:https://github.com/.../fw.bin
[Device downloads and flashes - 30-60 seconds]
Device â†’ Mac:  UPDATE_SUCCESS
[Device reboots]
[BLE reconnects]
Device â†’ Mac:  VERSION:1.3.0
```

---

## Error Handling

### Timeout Errors

If a command doesn't complete within the timeout:

```
Mac â†’ Device:  READ
Device â†’ Mac:  READY
[30 seconds pass, no tag]
Device â†’ Mac:  ERROR:Timeout
```

### Authentication Errors

If tag authentication fails:

```
Mac â†’ Device:  READ
Device â†’ Mac:  READY
Device â†’ Mac:  UID:12345678
Device â†’ Mac:  ERROR:Auth failed - check library patch
```

### Network Errors

If WiFi or download fails:

```
Mac â†’ Device:  CHECK_UPDATE
Device â†’ Mac:  ERROR:WiFi connection failed
```

---

## Message Length Limits

- Maximum command length: **256 characters**
- Maximum response length: **256 characters**
- BLE MTU: **23 bytes** (default) to **512 bytes** (negotiated)

For long messages (like WRITE commands), BLE will automatically fragment.

---

## Connection Lifecycle

### 1. Initial Connection

```
[Mac starts scanning]
[Device advertises as "CFS-Programmer"]
[Mac connects]
[Device sends VERSION automatically]
Device â†’ Mac:  VERSION:1.2.0
```

### 2. During Use

```
[Mac sends commands as needed]
[Device responds immediately or when ready]
```

### 3. Disconnection

```
[User closes app or moves out of range]
[BLE connection drops]
[Device returns to advertising]
```

### 4. Reconnection

```
[Mac reconnects automatically]
[Device sends VERSION again]
Device â†’ Mac:  VERSION:1.2.0
```

---

## Rate Limiting

No explicit rate limiting, but recommended:

- **Minimum time between commands:** 100ms
- **Maximum commands per second:** 10
- **Wait for response** before sending next command

---

## Future API Extensions

Possible additions in future versions:

```
GET_STATUS          â†’ Returns device health info
SET_LED:<color>     â†’ Manually control LED
GET_WIFI_STATUS     â†’ Check WiFi connection
FACTORY_RESET       â†’ Reset to defaults
```

---

## Versioning

API version follows firmware version:

- **v1.0.x:** Basic read functionality
- **v1.1.x:** Added write support
- **v1.2.x:** Added OTA updates (current)
- **v1.3.x:** (future) Enhanced features

---

## Implementation Example (Swift)

```swift
func sendCommand(_ command: String) {
    guard let data = command.data(using: .utf8),
          let rxChar = rxCharacteristic else { return }
    peripheral.writeValue(data, for: rxChar, type: .withResponse)
}

func handleResponse(_ message: String) {
    if message.hasPrefix("TAG_DATA:") {
        let parts = message.components(separatedBy: "|")
        // Parse material, length, color, serial
    } else if message.hasPrefix("ERROR:") {
        let error = message.replacingOccurrences(of: "ERROR:", with: "")
        // Show error to user
    }
}
```

---

## Testing

Use a BLE testing app like:
- **LightBlue** (iOS/macOS)
- **nRF Connect** (iOS/Android/Desktop)

Connect to service UUID and manually send commands to RX characteristic.

---

## Support

- **Protocol Version:** 1.2.0
- **Compatible Firmware:** v1.2.0+
- **Compatible Mac App:** v1.2.0+
- **BLE Version:** 4.0+