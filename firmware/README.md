# CFS Programmer Firmware

ESP32-S3 + PN532 NFC Reader Firmware

## Version

Current: **1.2.0** (2025-12-21)

## Features

- âœ… Read Creality CFS (Creality Filament System) tags
- âœ… Write dual tag sets (front + back tags with matching data)
- âœ… Bluetooth Low Energy (BLE) interface for Mac app
- âœ… WiFi OTA (Over-The-Air) firmware updates
- âœ… Automatic version reporting
- âœ… Blank tag detection
- âœ… UID-based authentication key generation
- âœ… AES-128 encryption/decryption
- âœ… LED status indicators
- âœ… OLED display feedback

## Hardware Requirements

| Component | Model | Interface | Notes |
|-----------|-------|-----------|-------|
| **Microcontroller** | ESP32-S3 | - | Any variant with WiFi + BLE |
| **NFC Reader** | PN532 | I2C | Must use I2C mode (not SPI) |
| **Display** | SH1106 | I2C | 128x64 OLED, shares I2C bus |
| **LED** | WS2812B | GPIO | Single addressable RGB LED |

### Pin Configuration

```cpp
#define I2C_SDA         8    // I2C Data (PN532 + OLED)
#define I2C_SCL         9    // I2C Clock
#define WS2812_PIN     48    // RGB LED data
#define PN532_IRQ      -1    // Not used
```

## Software Requirements

### Arduino IDE Setup

1. **Install ESP32 Board Support**
   - File â†’ Preferences â†’ Additional Board Manager URLs
   - Add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Tools â†’ Board â†’ Boards Manager â†’ Search "ESP32" â†’ Install

2. **Select Board**
   - Tools â†’ Board â†’ ESP32 Arduino â†’ ESP32S3 Dev Module

3. **Board Settings**
   - Upload Speed: 921600
   - USB CDC On Boot: Enabled
   - Flash Size: 4MB (or your module's size)
   - Partition Scheme: Default 4MB with spiffs

### Required Libraries

Install via Library Manager (Sketch â†’ Include Library â†’ Manage Libraries):

| Library | Version | Purpose |
|---------|---------|---------|
| Adafruit PN532 | 1.3.0+ | NFC reader control |
| U8g2 | 2.35.0+ | OLED display driver |
| Adafruit NeoPixel | 1.12.0+ | WS2812 LED control |
| AESLib | 2.1.0+ | AES encryption |
| ArduinoJson | 6.21.0+ | JSON parsing for GitHub API |
| ESP32 Core | 2.0.14+ | ESP32 platform support |

## CRITICAL: PN532 Library Patch

âš ï¸ **REQUIRED** - The Adafruit PN532 library has a timing bug that causes authentication failures.

### Instructions

1. Locate the library file:
   - **macOS:** `~/Documents/Arduino/libraries/Adafruit_PN532/Adafruit_PN532.cpp`
   - **Windows:** `Documents\Arduino\libraries\Adafruit_PN532\Adafruit_PN532.cpp`
   - **Linux:** `~/Arduino/libraries/Adafruit_PN532/Adafruit_PN532.cpp`

2. Open `Adafruit_PN532.cpp` in a text editor

3. Find line ~60:
   ```cpp
   #define PN532_PACKBUFFSIZE 64
   byte pn532_packetbuffer[PN532_PACKBUFFSIZE];
   ```

4. Add immediately after:
   ```cpp
   #define SLOWDOWN 1
   ```

5. Save file and restart Arduino IDE

### Why This Patch?

The PN532 chip has slower ACK frame responses than the library expects. The `SLOWDOWN` flag adds necessary timing delays to prevent authentication timeouts.

**Without this patch:** Authentication will fail ~80% of the time  
**With this patch:** Authentication works reliably

## Installation

### Quick Start

1. Install Arduino IDE and ESP32 support
2. Install required libraries
3. **Apply PN532 library patch** (critical!)
4. Open `CFS_Handheld_v1.2_OTA.ino`
5. Update GitHub username (line 32)
6. Connect ESP32-S3 via USB
7. Select board and port
8. Upload firmware

### Detailed Steps

```bash
# 1. Install Arduino CLI (optional, for command line)
brew install arduino-cli  # macOS
# or download from https://arduino.github.io/arduino-cli/

# 2. Install ESP32 core
arduino-cli core install esp32:esp32@2.0.14

# 3. Install libraries
arduino-cli lib install "Adafruit PN532@1.3.1"
arduino-cli lib install "U8g2@2.35.9"
arduino-cli lib install "Adafruit NeoPixel@1.12.0"
arduino-cli lib install "AESLib@2.1.0"
arduino-cli lib install "ArduinoJson@6.21.3"

# 4. Apply library patch (manual step)

# 5. Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 CFS_Handheld_v1.2_OTA/

# 6. Upload
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:esp32s3 CFS_Handheld_v1.2_OTA/
```

## Configuration

### GitHub Repository URL

Edit line 32 in `CFS_Handheld_v1.2_OTA.ino`:

```cpp
#define GITHUB_API_URL "https://api.github.com/repos/YOUR_USERNAME/cfs-programmer/releases/latest"
```

Replace `YOUR_USERNAME` with your actual GitHub username.

### WiFi Credentials (Runtime)

WiFi is configured at runtime via the Mac app:
1. Open Mac app Settings
2. Click "Configure WiFi"
3. Enter SSID and password
4. Credentials are sent to device via BLE

**No hardcoding needed!**

## Usage

### First Boot

1. Power on device
2. Serial output shows:
   ```
   â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—
   â•‘   CFS Programmer v1.2.0               â•‘
   â•‘   ESP32-S3 + PN532                    â•‘
   â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

   [1/5] Initializing RGB LED...
   [2/5] Initializing I2C bus...
   [3/5] Initializing OLED...
   [4/5] Initializing PN532...
   [5/5] Initializing BLE...

   ðŸŽ‰ READY!
   ```

3. OLED shows: "Ready! v1.2.0 Waiting..."
4. BLE advertising starts (name: "CFS-Programmer")

### Connecting Mac App

1. Mac app auto-scans for device
2. When found, connects automatically
3. Device sends: `VERSION:1.2.0`
4. Mac app displays firmware version
5. Ready for tag operations

### Reading a Tag

1. Mac app sends: `READ`
2. Device responds: `READY`
3. Place tag on reader
4. Device detects UID: `UID:60ea1221`
5. Authenticates and reads blocks 4-6
6. Decrypts with AES
7. Parses data
8. Sends: `TAG_DATA:PLA|330m|#FFFFFF|S/N:123456`

### Writing Tags

1. Mac app sends: `WRITE:<48-char-cfs-data>`
2. Device responds: `WRITE_READY`
3. Place first tag â†’ writes blocks â†’ `TAG1_WRITTEN`
4. Place second tag â†’ writes blocks â†’ `TAG2_WRITTEN`
5. Both tags now have identical data

### OTA Updates

1. Mac app configures WiFi
2. Mac app sends: `CHECK_UPDATE`
3. Device:
   - Connects to WiFi
   - Fetches GitHub API
   - Compares versions
4. If update exists: `UPDATE_AVAILABLE:1.3.0,<url>`
5. Mac app sends: `OTA_UPDATE:<url>`
6. Device downloads and flashes firmware
7. Reboots with new version

## LED Indicators

| Color | Status |
|-------|--------|
| **Off** | Idle |
| **Blue** | Waiting for tag / Reading |
| **Yellow** | Processing / Authenticating |
| **Green** | Success |
| **Red** | Error |
| **Cyan** | BLE connected |
| **Magenta** | OTA update in progress |

## Serial Monitor Output

Baud rate: **115200**

Example output when reading a tag:

```
ðŸ“– READ mode - place tag

â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
âœ… TAG DETECTED! UID (4 bytes): 60 EA 12 21
â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

ðŸ” Generating MIFARE key from UID...
   UID repeated: 60 EA 12 21 60 EA 12 21 60 EA 12 21 60 EA 12 21
   Encrypted:    1F 1E 83 A9 71 82 91 C2 80 DF FD EF 8D F7 58 66
   MIFARE Key:   1F 1E 83 A9 71 82

ðŸ”‘ Authenticating (with retry logic)...
   [1] Default Key A on Block 7... âŒ
   [2] Custom Key A on Block 7... âœ… SUCCESS!

ðŸ“„ Reading data blocks...
   Block 4: 73 C9 53 87 D6 F5 78 C5 3E CF 1A 0B 2B 43 27 76 âœ…
   Block 5: 89 B1 9B A9 4B 8B FC 5F 97 F2 BC B1 DA 9A CF 4B âœ…
   Block 6: 31 A4 93 68 5D E9 93 18 7E C8 28 82 96 71 F8 21 âœ…

ðŸ”“ Decrypting with d_key...

âœ… TAG READ COMPLETE!
   Material:  PLA
   Length:    330 meters
   Color:     #FFFFFF
```

## Troubleshooting

### Authentication Fails

**Symptom:** `ERROR:Auth failed - check library patch`

**Solutions:**
1. âœ… Verify `SLOWDOWN 1` is in `Adafruit_PN532.cpp`
2. âœ… Check I2C connections (SDA, SCL, GND, VCC)
3. âœ… Try reducing I2C speed: `Wire.setClock(50000);`
4. âœ… Power cycle PN532 module

### PN532 Not Found

**Symptom:** `ERROR! PN532 not found`

**Solutions:**
1. Check I2C wiring
2. Verify PN532 is in I2C mode (check DIP switches)
3. Try different I2C address scanner
4. Check power supply (PN532 needs 3.3V or 5V)

### BLE Won't Connect

**Symptom:** Mac app stuck on "Searching..."

**Solutions:**
1. Restart ESP32
2. Check Bluetooth is enabled on Mac
3. Forget device in Mac Bluetooth settings and reconnect
4. Verify BLE advertising is active (check Serial Monitor)

### OTA Update Fails

**Symptom:** Update hangs or errors

**Solutions:**
1. âœ… Check WiFi credentials are correct
2. âœ… Verify WiFi signal strength
3. âœ… Ensure GitHub release has `.bin` file
4. âœ… Check GitHub API URL is correct
5. âœ… Monitor Serial output for detailed error

### Tags Won't Write

**Symptom:** `ERROR:Write failed`

**Solutions:**
1. Use blank MIFARE Classic 1K tags
2. Ensure tags aren't write-protected
3. Check if tag is genuine MIFARE (not ultralight)
4. Try different tag brand

## Development

### Modifying Firmware

1. Edit `CFS_Handheld_v1.2_OTA.ino`
2. Update `FIRMWARE_VERSION` constant
3. Test on hardware
4. Commit to git
5. Tag release: `git tag fw-v1.3.0`
6. Push: `git push origin fw-v1.3.0`
7. GitHub Actions builds and releases

### Adding Features

Common modifications:

**Add new material type:**
```cpp
// In readTag() function
else if (filmID == "101006") material = "PC";
```

**Change timeout:**
```cpp
// In loop() function
if (millis() - readingStartTime > 60000) {  // 60 seconds instead of 30
```

**Add custom LED pattern:**
```cpp
void blinkPattern() {
  for (int i = 0; i < 3; i++) {
    setLED(WS_GREEN);
    delay(100);
    setLED(WS_OFF);
    delay(100);
  }
}
```

## Version History

### v1.2.0 (2025-12-21)
- âœ… Added OTA update support via WiFi
- âœ… Added firmware versioning system
- âœ… Added WiFi configuration over BLE
- âœ… Added GitHub releases integration
- âœ… Added blank tag detection
- âœ… Improved error handling

### v1.1.0 (2025-12-20)
- âœ… Added authentication retry logic
- âœ… Added blank tag detection
- âœ… Improved serial output formatting
- âœ… Fixed timing issues with PN532

### v1.0.0 (2025-12-19)
- âœ… Initial release
- âœ… Read CFS tags
- âœ… Basic BLE interface
- âœ… OLED display support

## License

MIT License - See LICENSE file in repository

## Support

- **Documentation:** See `docs/` folder
- **Issues:** GitHub Issues
- **Hardware:** Works with ESP32-S3 + PN532 (I2C)
- **Tags:** MIFARE Classic 1K only

## Credits

Developed for the maker community to work with Creality CFS tags.

Use responsibly and clone only your own tags for backup purposes.