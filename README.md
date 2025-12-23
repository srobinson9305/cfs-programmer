# CFS Programmer

A complete system for reading and writing Creality CFS (Creality Filament System) NFC tags used in K2 Plus/Pro 3D printers.

![Version](https://img.shields.io/badge/version-1.2.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-orange)

## Overview

This project provides both hardware and software to read, write, and clone Creality CFS tags. Each spool of Creality filament comes with two NFC tags (front and back) containing encrypted information about the filament type, color, length, and serial number.

### Features

- âœ… **Read CFS Tags** - Decode all tag information (material, length, color, serial)
- âœ… **Write Dual Tags** - Program matching front/back tag pairs
- âœ… **Blank Tag Detection** - Identify empty tags ready for writing
- âœ… **Mac App Interface** - User-friendly macOS application
- âœ… **OTA Updates** - Update firmware wirelessly from GitHub releases
- âœ… **Material Database** - Expandable database of filament types
- âœ… **AES Encryption** - Full encryption/decryption support
- âœ… **BLE Communication** - Wireless connection to Mac app

## Hardware Requirements

| Component | Specification | Notes |
|-----------|---------------|-------|
| **Microcontroller** | ESP32-S3 | Any variant with WiFi + BLE |
| **NFC Reader** | PN532 | I2C mode only |
| **Display** | SH1106 128x64 OLED | I2C |
| **LED** | WS2812B RGB | Single addressable LED |
| **Tags** | MIFARE Classic 1K | For writing new tags |

### Wiring

```
PN532 NFC Reader:
  SDA â†’ ESP32 GPIO 8
  SCL â†’ ESP32 GPIO 9
  VCC â†’ 3.3V
  GND â†’ GND

SH1106 OLED:
  SDA â†’ ESP32 GPIO 8 (shared with PN532)
  SCL â†’ ESP32 GPIO 9 (shared with PN532)
  VCC â†’ 3.3V
  GND â†’ GND

WS2812B LED:
  DIN â†’ ESP32 GPIO 48
  VCC â†’ 5V
  GND â†’ GND
```

## Software Components

### 1. Firmware (ESP32-S3)

Located in `firmware/`

- Written in Arduino/C++
- Handles NFC read/write operations
- BLE communication with Mac app
- OTA firmware updates via WiFi
- Version: 1.2.0

[Firmware Documentation â†’](firmware/README.md)

### 2. Mac App

Located in `mac-app/`

- Native SwiftUI application
- Connects via Bluetooth LE
- Material database management
- WiFi configuration
- OTA update manager
- Version: 1.2.0

### 3. Documentation

Located in `docs/`

- `FIRMWARE.md` - Complete firmware documentation
- `TAG_FORMAT.md` - CFS tag format specification
- `API.md` - BLE protocol reference
- `GITHUB_SETUP.md` - OTA update setup guide

## Quick Start

### 1. Hardware Setup

1. Wire components according to diagram above
2. Ensure PN532 is in **I2C mode** (check DIP switches)
3. Power via USB-C

### 2. Firmware Installation

```bash
# Clone repository
git clone https://github.com/yourusername/cfs-programmer.git
cd cfs-programmer

# Open firmware in Arduino IDE
open firmware/CFS_Handheld_v1.2_OTA/CFS_Handheld_v1.2_OTA.ino

# CRITICAL: Apply library patch (see firmware/README.md)

# Install required libraries via Library Manager:
# - Adafruit PN532
# - U8g2
# - Adafruit NeoPixel
# - AESLib
# - ArduinoJson

# Select board: ESP32S3 Dev Module
# Upload to device
```

**âš ï¸ IMPORTANT:** You must apply the PN532 library patch or authentication will fail. See [firmware/README.md](firmware/README.md) for details.

### 3. Mac App Installation

```bash
# Open Xcode project
open mac-app/CFS\ Programmer.xcodeproj

# Build and run (âŒ˜R)
```

### 4. First Use

1. Power on ESP32 device
2. Open Mac app
3. App auto-connects to device
4. Configure WiFi (Settings â†’ WiFi Setup)
5. Ready to read/write tags!

## Usage

### Reading a Tag

1. Click **Read Tag** in Mac app
2. Place CFS tag on PN532 reader
3. Tag information displays immediately
4. View material, length, color, and serial number

### Writing Tags

1. Click **Write Tags** in Mac app
2. Select material from database
3. Choose weight (250g, 500g, 1kg, etc.)
4. Pick color
5. Enter serial (or auto-generate)
6. Click **Write Both Tags**
7. Place first blank tag â†’ writes
8. Place second blank tag â†’ writes
9. Done! Both tags have identical data

### OTA Updates

1. Settings â†’ WiFi Setup â†’ Configure network
2. Settings â†’ Check for Updates
3. If available, click Install Update
4. Wait 30-60 seconds
5. Device reboots with new firmware

## Project Structure

```
cfs-programmer/
â”œâ”€â”€ firmware/                      # ESP32 firmware
â”‚   â”œâ”€â”€ CFS_Handheld_v1.2_OTA/
â”‚   â”‚   â””â”€â”€ CFS_Handheld_v1.2_OTA.ino
â”‚   â””â”€â”€ README.md
â”œâ”€â”€ mac-app/                       # macOS application
â”‚   â”œâ”€â”€ CFS Programmer.xcodeproj
â”‚   â””â”€â”€ CFS Programmer/
â”‚       â”œâ”€â”€ ContentView.swift
â”‚       â”œâ”€â”€ CFS_ProgrammerApp.swift
â”‚       â””â”€â”€ Assets.xcassets
â”œâ”€â”€ hardware/                      # Hardware documentation
â”‚   â””â”€â”€ wiring-diagrams/
â”œâ”€â”€ docs/                          # Documentation
â”‚   â”œâ”€â”€ FIRMWARE.md
â”‚   â”œâ”€â”€ TAG_FORMAT.md
â”‚   â”œâ”€â”€ API.md
â”‚   â””â”€â”€ GITHUB_SETUP.md
â”œâ”€â”€ .github/
â”‚   â””â”€â”€ workflows/
â”‚       â””â”€â”€ release-firmware.yml   # Auto-build on release
â”œâ”€â”€ .gitignore
â”œâ”€â”€ README.md
â””â”€â”€ LICENSE
```

## How It Works

### CFS Tag Format

CFS tags are **MIFARE Classic 1K** NFC tags containing 48 bytes of encrypted data:

```
[Date][Vendor][??][Film ID][Color ][Length][Serial][Reserved]
  5     4      2     6        7      4      6       14 bytes
```

Example:
```
ABC2112 0276 A2 101001 0FFFFFF 0330 123456 00000000000000
â”‚       â”‚    â”‚  â”‚      â”‚       â”‚    â”‚      â””â”€ Reserved
â”‚       â”‚    â”‚  â”‚      â”‚       â”‚    â””â”€ Serial number
â”‚       â”‚    â”‚  â”‚      â”‚       â””â”€ Length (816m hex)
â”‚       â”‚    â”‚  â”‚      â””â”€ Color (#FFFFFF)
â”‚       â”‚    â”‚  â””â”€ Film ID (PLA = 101001)
â”‚       â”‚    â””â”€ Batch code
â”‚       â””â”€ Vendor (Creality = 0276)
â””â”€ Date code
```

### Encryption

- **Algorithm:** AES-128 CBC
- **Keys:** Hardcoded (same as Creality printers)
- **IV:** All zeros
- **UID-based authentication:** MIFARE keys derived from tag UID

[Full Tag Format Documentation â†’](docs/TAG_FORMAT.md)

## Compatible Printers

- âœ… Creality K2 Plus
- âœ… Creality K2 Pro
- âš ï¸ Creality K1 Series (may work, needs testing)
- âš ï¸ CR-Series with CFS retrofit (untested)

## Compatible Tags

For writing new tags:

- âœ… **MIFARE Classic 1K** (NXP original)
- âœ… **MIFARE Classic 1K S50** (Chinese clones)
- âœ… **FM11RF08** (compatible IC)
- âŒ MIFARE Ultralight (too small)
- âŒ NTAG215/216 (different protocol)

## Development

### Building Firmware

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/CFS_Handheld_v1.2_OTA/
```

### Creating a Release

```bash
# Update version in firmware code
# Commit changes
git add firmware/
git commit -m "Release firmware v1.3.0"

# Create tag
git tag -a fw-v1.3.0 -m "Firmware v1.3.0 - New features"

# Push (triggers GitHub Actions)
git push origin main
git push origin fw-v1.3.0
```

GitHub Actions automatically builds and creates release with .bin file.

### Testing

1. **Hardware:** Test on real ESP32 + PN532
2. **Tags:** Test with genuine Creality tags and blanks
3. **BLE:** Test Mac app connection and commands
4. **OTA:** Test update from previous version

## Troubleshooting

### Tags won't authenticate

**Solution:** Apply the PN532 library patch (see firmware/README.md). This is the #1 issue.

### BLE won't connect

**Solutions:**
- Restart ESP32 device
- Restart Mac Bluetooth
- Check device is advertising (Serial Monitor)

### OTA update fails

**Solutions:**
- Verify WiFi credentials
- Check WiFi signal strength
- Ensure GitHub release has .bin file
- Monitor Serial output for errors

[Full Troubleshooting Guide â†’](docs/FIRMWARE.md#troubleshooting)

## Known Issues

- PN532 library timing bug (fixed with SLOWDOWN patch)
- Some Chinese MIFARE clones may have authentication issues
- First BLE connection after boot can be slow (~5 seconds)

## Roadmap

### v1.3.0 (Planned)
- [ ] ESP32-2432S028R support (touchscreen version)
- [ ] Write function implementation in firmware
- [ ] Tag history/database in Mac app
- [ ] Batch write mode (multiple sets)

### v2.0.0 (Future)
- [ ] iOS app version
- [ ] Web interface for device
- [ ] Material profiles from cloud
- [ ] Tag usage analytics

## Contributing

Contributions welcome!

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open Pull Request

### Areas to Contribute

- ESP32-2432S028R migration
- iOS app development
- Additional material profiles
- Documentation improvements
- Testing with different tag brands

## License

MIT License

Copyright (c) 2025 CFS Programmer Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Legal Notice

This project is for educational and personal use only. Use responsibly:

- âœ… Clone your own tags for backup
- âœ… Create custom tags for personal use
- âŒ Don't counterfeit commercial tags
- âŒ Don't violate Creality's intellectual property

The developers are not responsible for misuse of this software.

## Acknowledgments

- Creality for creating an interesting NFC tag system
- Adafruit for the PN532 library
- ESP32 community for excellent documentation
- All contributors and testers

## Support

- **Documentation:** See `docs/` folder
- **Issues:** [GitHub Issues](https://github.com/yourusername/cfs-programmer/issues)
- **Discussions:** [GitHub Discussions](https://github.com/yourusername/cfs-programmer/discussions)

## Authors

Created by the maker community for the maker community.

## See Also

- [Firmware Documentation](firmware/README.md)
- [Tag Format Specification](docs/TAG_FORMAT.md)
- [API Reference](docs/API.md)
- [OTA Setup Guide](docs/GITHUB_SETUP.md)

---

**Made with â¤ï¸ for the 3D printing community**