# CFS Programmer Firmware Documentation
Version 1.2.0

## Overview

The CFS Programmer is an ESP32-S3 based NFC reader/writer designed to read and write Creality CFS (Creality Filament System) tags used in K2 Plus and similar 3D printers.

## Hardware Requirements

- **MCU:** ESP32-S3 (any variant with WiFi + BLE)
- **NFC Reader:** PN532 (I2C mode)
- **Display:** SH1106 128x64 OLED (I2C)
- **LED:** WS2812B RGB LED
- **Connections:**
  - I2C SDA: GPIO 8
  - I2C SCL: GPIO 9
  - WS2812: GPIO 48
  - PN532 IRQ: Not used (-1)

## Pin Configuration

```cpp
#define I2C_SDA         8
#define I2C_SCL         9
#define WS2812_PIN     48
#define PN532_IRQ      -1
```

## Library Requirements

Install these libraries via Arduino IDE Library Manager:

1. **Adafruit PN532** (v1.3.0+)
2. **U8g2** (v2.35.0+)
3. **Adafruit NeoPixel** (v1.12.0+)
4. **AESLib** (v2.1.0+)
5. **ArduinoJson** (v6.21.0+)
6. **ESP32 Core** (v2.0.14+)

## Critical Library Patch

**REQUIRED:** The Adafruit PN532 library has a timing bug that causes authentication failures.

### Fix Instructions:

1. Locate `Adafruit_PN532.cpp` in your libraries folder:
   - macOS: `~/Documents/Arduino/libraries/Adafruit_PN532/`
   - Windows: `Documents\Arduino\libraries\Adafruit_PN532\`

2. Open `Adafruit_PN532.cpp` in a text editor

3. Find line ~60 that contains:
   ```cpp
   #define PN532_PACKBUFFSIZE 64
   byte pn532_packetbuffer[PN532_PACKBUFFSIZE];
   ```

4. Add this line immediately after:
   ```cpp
   #define SLOWDOWN 1
   ```

5. Save and restart Arduino IDE

**Why?** The PN532 chip has slower ACK frame responses than the library expects. This flag adds necessary timing delays.

## Firmware Version System

### Version Format

Versions follow Semantic Versioning: `MAJOR.MINOR.PATCH`

- **MAJOR:** Breaking changes (e.g., 1.x.x → 2.0.0)
- **MINOR:** New features, backward compatible (e.g., 1.1.0 → 1.2.0)
- **PATCH:** Bug fixes (e.g., 1.2.0 → 1.2.1)

### Updating Version

Edit this line in the firmware:

```cpp
#define FIRMWARE_VERSION "1.2.0"
```

The build date/time are auto-generated:

```cpp
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__
```

## BLE Protocol

### Service and Characteristics

- **Service UUID:** `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- **TX Characteristic (notify):** `1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e`
- **RX Characteristic (write):** `beb5483e-36e1-4688-b7f5-ea07361b26a8`

### Commands (RX - Mac → Device)

| Command | Format | Description |
|---------|--------|-------------|
| `READ` | `READ` | Start reading mode |
| `WRITE` | `WRITE:<48-char-hex>` | Write CFS data to dual tags |
| `CANCEL` | `CANCEL` | Cancel current operation |
| `GET_VERSION` | `GET_VERSION` | Request firmware version |
| `WIFI_CONFIG` | `WIFI_CONFIG:<SSID>,<PASSWORD>` | Configure WiFi credentials |
| `CHECK_UPDATE` | `CHECK_UPDATE` | Check GitHub for firmware updates |
| `OTA_UPDATE` | `OTA_UPDATE:<url>` | Perform OTA update from URL |

### Responses (TX - Device → Mac)

| Response | Format | Description |
|----------|--------|-------------|
| Version | `VERSION:1.2.0` | Firmware version (sent on connect) |
| Ready | `READY` | Ready for tag operation |
| UID | `UID:60ea1221` | Tag UID detected (hex, lowercase) |
| Tag Data | `TAG_DATA:PLA\|165m\|#FFFFFF\|S/N:123456` | Successfully read tag |
| Blank Tag | `BLANK_TAG` | Tag is blank (all zeros) |
| Error | `ERROR:<message>` | Error occurred |
| Write Ready | `WRITE_READY` | Ready to write tags |
| Tag 1 Written | `TAG1_WRITTEN` | First tag written successfully |
| Tag 2 Written | `TAG2_WRITTEN` | Second tag written successfully |
| WiFi OK | `WIFI_OK` | WiFi configured successfully |
| Update Available | `UPDATE_AVAILABLE:<version>,<url>` | New firmware available |
| Up to Date | `UP_TO_DATE` | Firmware is current |
| Update Success | `UPDATE_SUCCESS` | OTA update completed |

## LED Status Indicators

| Color | Meaning |
|-------|---------|
| Off | Idle |
| Blue | Reading tag / Waiting for tag |
| Yellow | Processing |
| Green | Success |
| Red | Error |
| Cyan | BLE connected |
| Magenta | OTA update in progress |

## OTA Update Process

### WiFi Configuration

1. Mac app sends WiFi credentials:
   ```
   WIFI_CONFIG:MyNetwork,MyPassword123
   ```

2. Device stores credentials and responds:
   ```
   WIFI_OK
   ```

### Update Check

1. Mac app requests update check:
   ```
   CHECK_UPDATE
   ```

2. Device:
   - Connects to WiFi
   - Fetches GitHub API: `https://api.github.com/repos/USERNAME/REPO/releases/latest`
   - Parses JSON for latest version tag
   - Compares with current version

3. Response if update available:
   ```
   UPDATE_AVAILABLE:1.3.0,https://github.com/.../firmware.bin
   ```

4. Or if up to date:
   ```
   UP_TO_DATE
   ```

### Performing Update

1. Mac app sends update command:
   ```
   OTA_UPDATE:https://github.com/.../firmware.bin
   ```

2. Device:
   - Connects to WiFi
   - Downloads .bin file via HTTPS
   - Writes to OTA partition using ESP32 Update library
   - Verifies integrity
   - Reboots if successful

3. After reboot, new version sends:
   ```
   VERSION:1.3.0
   ```

## State Machine

The firmware operates in 4 states:

```
STATE_IDLE
  ↓ (READ command)
STATE_READING
  ↓ (tag detected)
  → Read blocks
  → Decrypt
  → Parse data
  → Send TAG_DATA
  ↓
STATE_IDLE

STATE_IDLE
  ↓ (WRITE command)
STATE_WRITING
  ↓ (tag 1 detected)
  → Encrypt
  → Write blocks
  → Send TAG1_WRITTEN
  ↓ (tag 2 detected)
  → Write blocks
  → Send TAG2_WRITTEN
  ↓
STATE_IDLE

STATE_IDLE
  ↓ (OTA_UPDATE command)
STATE_UPDATING
  ↓ (download + flash)
  → Reboot
```

## Error Handling

Common errors and responses:

| Error | Cause | Response |
|-------|-------|----------|
| Auth failed | PN532 authentication timeout | `ERROR:Auth failed - check library patch` |
| Not Creality | Non-CFS tag or wrong format | `ERROR:Not a Creality CFS Tag` |
| WiFi failed | Can't connect to network | `ERROR:WiFi connection failed` |
| Download failed | HTTP error during OTA | `ERROR:Download failed` |
| Timeout | No tag presented within 30s | `ERROR:Timeout` |

## Troubleshooting

### Tag Authentication Fails

**Symptom:** `ERROR:Auth failed` even with library patch

**Solutions:**
1. Verify `SLOWDOWN 1` is added to `Adafruit_PN532.cpp`
2. Check I2C connections (SDA, SCL, GND, VCC)
3. Reduce I2C speed: `Wire.setClock(50000);` instead of 100000
4. Power cycle the PN532 module

### OTA Update Hangs

**Symptom:** Device stuck at "Updating..."

**Solutions:**
1. Check WiFi signal strength
2. Verify GitHub release has `.bin` file attached
3. Ensure enough flash space (use `Update.begin(contentLength)`)
4. Check Serial Monitor for detailed error messages

### BLE Disconnects

**Symptom:** Mac app loses connection frequently

**Solutions:**
1. Keep devices within 10 meters
2. Avoid interference from other 2.4GHz devices
3. Power ESP32 from good USB supply (500mA minimum)
4. Check for metal objects between devices

## Compilation

### Arduino IDE

1. Install ESP32 board support:
   - File → Preferences → Additional Board Manager URLs:
   - Add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`

2. Install board:
   - Tools → Board → Boards Manager
   - Search "ESP32"
   - Install "esp32 by Espressif Systems"

3. Select board:
   - Tools → Board → ESP32 Arduino → ESP32S3 Dev Module

4. Configure settings:
   - Upload Speed: 921600
   - USB CDC On Boot: Enabled
   - Flash Size: 4MB (or your module size)
   - Partition Scheme: Default 4MB with spiffs

5. Compile and upload

### Arduino CLI

```bash
# Install ESP32 core
arduino-cli core install esp32:esp32

# Install libraries
arduino-cli lib install "Adafruit PN532"
arduino-cli lib install "U8g2"
arduino-cli lib install "Adafruit NeoPixel"
arduino-cli lib install "AESLib"
arduino-cli lib install "ArduinoJson"

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 \
  firmware/CFS_Handheld_v1.2_OTA/

# Upload
arduino-cli upload -p /dev/cu.usbserial-* \
  --fqbn esp32:esp32:esp32s3 \
  firmware/CFS_Handheld_v1.2_OTA/
```

## Future Hardware Migration

### ESP32-2432S028R ("Cheap Yellow Display")

When migrating to the touchscreen version:

**Compatible:**
- BLE code (no changes)
- PN532 NFC reader (still I2C)
- AES encryption (no changes)
- WiFi/OTA (same ESP32 core)

**Changes needed:**
- Display library: `U8g2` → `TFT_eSPI`
- Display size: 128x64 → 240x320
- Add touch support: `XPT2046_Touchscreen`
- Built-in RGB LED (simpler wiring)
- Built-in WiFi (no external antenna needed)

Estimated migration effort: 2-3 hours (mostly UI code)
