# Creality CFS RFID Programmer

A handheld RFID tag reader/writer for Creality's CFS (Creality Filament System) compatible with K2 Pro and other CFS-enabled 3D printers. This project allows you to read, write, and manage filament spool tags with custom profiles.

![Project Status](https://img.shields.io/badge/status-in%20development-yellow)
![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-ESP32%20%7C%20macOS-lightgrey)

## 🎯 Features

### Current Features (Reader)
- ✅ Read Creality CFS RFID tags (MIFARE Classic 1K)
- ✅ Decrypt and parse tag data (material type, color, length, serial)
- ✅ UID-based authentication with AES-128 encryption
- ✅ Support for ESP32-S3 with RC522 RFID module
- ✅ Complete tag data parsing and display

### Planned Features (Writer)
- 🚧 Write custom CFS tags for any filament
- 🚧 Dual-tag writing workflow (front/back labels)
- 🚧 Verify-after-write functionality
- 🚧 macOS SwiftUI app for tag management
- 🚧 BLE communication between handheld and Mac
- 🚧 Filament database with custom profiles
- 🚧 K2 Pro integration via SSH/SCP
- 🚧 Sync with Creality Print 6 profiles

## 📋 Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Quick Start](#quick-start)
- [Project Structure](#project-structure)
- [Tag Format](#tag-format)
- [Usage](#usage)
- [Development](#development)
- [Contributing](#contributing)
- [License](#license)
- [Credits](#credits)

## 🔧 Hardware Requirements

### ESP32-S3 Reader Setup
- **Microcontroller:** ESP32-S3 DevKit (or similar)
- **RFID Module:** RC522 RFID Reader (SPI interface)
- **Power:** USB-C or 3.7V LiPo battery
- **Optional:** Status LED (WS2812B), OLED display

### Wiring Diagram (ESP32-S3 + RC522)

\`\`\`
RC522 Module     →    ESP32-S3
─────────────────────────────────
VCC (3.3V)       →    3.3V
GND              →    GND
MISO             →    GPIO 36
MOSI             →    GPIO 35
SCK              →    GPIO 39
SDA (SS)         →    GPIO 38
RST              →    GPIO 40
\`\`\`

### Alternative Hardware
The firmware also supports:
- NodeMCU ESP-32S (standard ESP32)
- Other ESP32 variants with SPI support

## 💻 Software Requirements

### Firmware
- **Arduino IDE:** 1.8.19 or newer (or Arduino IDE 2.x)
- **ESP32 Board Support:** via Board Manager
- **Libraries:**
  - \`MFRC522\` (by GithubCommunity)
  - \`AESLib\` (by suculent/THiNX - ESP32 compatible fork)
  - \`SPI\` (built-in)

### macOS App (Coming Soon)
- **macOS:** 13.0 (Ventura) or newer
- **Xcode:** 15.0+
- **Swift:** 5.9+

## 🚀 Quick Start

### 1. Hardware Setup

1. Wire the RC522 module to your ESP32-S3 according to the diagram above
2. Connect ESP32-S3 to your computer via USB
3. Verify the RFID module is detected (check I2C scanner if needed)

### 2. Firmware Installation

1. **Install Arduino IDE and ESP32 Support:**
   \`\`\`bash
   # Add ESP32 board support URL in Arduino IDE:
   # File → Preferences → Additional Board Manager URLs
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   \`\`\`

2. **Install Required Libraries:**
   - Open Arduino IDE
   - Go to **Sketch → Include Library → Manage Libraries**
   - Search and install:
     - \`MFRC522\`
     - \`AESLib\` (by suculent/THiNX)

3. **Configure Board Settings:**
   - Board: \`ESP32S3 Dev Module\`
   - Upload Speed: \`921600\`
   - USB CDC On Boot: \`Enabled\`
   - Port: Select your ESP32-S3 port

4. **Upload Firmware:**
   \`\`\`bash
   # Open the sketch
   cd firmware/CFS_Tag_Reader/
   # Open CFS_Tag_Reader.ino in Arduino IDE
   # Click Upload button or Ctrl+U
   \`\`\`

### 3. Test the Reader

1. Open Serial Monitor (115200 baud)
2. Place a Creality CFS tag on the RC522 reader
3. You should see output like:

\`\`\`
========================================
✅ TAG DETECTED!
========================================
UID: 60 EA 12 21
Generated Key: 1F 1E 83 A9 71 82

✅ READ SUCCESS!
========================================
Raw  AB1240276A21010010FFFFFF0165000001000000

╔════════════════════════════════════════╗
║       CFS TAG INFORMATION              ║
╚════════════════════════════════════════╝
Date:         AB124 (Oct 11, 2024)
Vendor ID:    0276 (Creality)
Batch:        A2
Filament ID:  101001 (PLA)
Color:        0FFFFFF (RGB: #FFFFFF)
Length:       0165 (165m / 500g)
Serial:       000001
\`\`\`

## 📁 Project Structure

\`\`\`
creality-cfs-programmer/
├── README.md                          # This file
├── LICENSE                            # MIT License
├── docs/                              # Documentation
│   ├── cfs-tag-format.md             # Complete CFS format specification
│   ├── hardware-setup.md             # Detailed wiring guides
│   ├── ble-protocol.md               # BLE command protocol (planned)
│   └── images/                       # Photos and diagrams
├── firmware/                          # Arduino/ESP32 firmware
│   ├── CFS_Tag_Reader/               # Main reader sketch
│   │   ├── CFS_Tag_Reader.ino       # Main program
│   │   └── README.md                 # Firmware documentation
│   └── examples/                     # Example sketches
│       └── I2C_Scanner/              # I2C device detection
├── macos-app/                         # macOS SwiftUI app (coming soon)
│   └── README.md
├── tools/                             # Utilities and scripts
│   └── decrypt-tag.py                # Python tag decryption tool
└── examples/                          # Sample data
    └── sample-tags.json              # Example tag data
\`\`\`

## 🏷️ Tag Format

CFS tags use **MIFARE Classic 1K** with custom AES-128 encryption:

### Data Structure (48 ASCII characters)

\`\`\`
Position:  0-4   5-8   9-10  11-16   17-23    24-27  28-33   34-39   40-47
Field:     Date  Vend  Batch MatID   Color    Length Serial  Reserve Padding
Length:    5     4     2     6       7        4      6       6       8
Example:   AB124 0276  A2    101001  0FFFFFF  0165   000001  000000  00000000
\`\`\`

### Encryption Keys

Two AES-128 keys are used:
- **u_key:** Generates MIFARE authentication key from UID
- **d_key:** Encrypts/decrypts tag data (ECB mode)

See [\`docs/cfs-tag-format.md\`](docs/cfs-tag-format.md) for complete specifications.

## 📖 Usage

### Reading Tags

Simply place a CFS tag on the reader. The firmware will:
1. Detect the tag and read its UID
2. Generate the authentication key from the UID
3. Authenticate to the secure sectors
4. Read and decrypt blocks 4-6
5. Parse and display all tag information

### Writing Tags (Coming Soon)

The writer functionality will allow:
- Creating tags for any filament brand/type
- Custom material profiles
- Dual-tag writing (front/back of spool)
- Automatic verification

## 🛠️ Development

### Building from Source

\`\`\`bash
# Clone the repository
git clone https://github.com/yourusername/creality-cfs-programmer.git
cd creality-cfs-programmer

# Firmware
cd firmware/CFS_Tag_Reader
# Open in Arduino IDE and upload

# macOS App (when available)
cd macos-app
open CFSProgrammer.xcodeproj
# Build in Xcode
\`\`\`

### Debugging

Enable verbose output in the firmware:
\`\`\`cpp
#define DEBUG_MODE true  // Add to top of .ino file
\`\`\`

View detailed encryption/decryption steps:
\`\`\`bash
# Serial monitor will show:
# - UID processing
# - Key generation
# - Block-by-block decryption
# - Raw hex data
\`\`\`

### Testing

Test with the provided sample tags in \`examples/sample-tags.json\`:
\`\`\`json
{
  "tag1": {
    "uid": "60EA1221",
    "encrypted_data": "73C95387D6F578C5...",
    "expected_output": "AB1240276A21010010FFFFFF0165000001000000"
  }
}
\`\`\`

## 🤝 Contributing

Contributions are welcome! This project is in active development.

### Priority Areas
1. **Hardware testing:** Verify compatibility with different ESP32 boards
2. **Tag writing:** Implement safe write operations
3. **macOS app:** SwiftUI interface and BLE communication
4. **Documentation:** Improve setup guides and troubleshooting

### How to Contribute

1. Fork the repository
2. Create a feature branch (\`git checkout -b feature/amazing-feature\`)
3. Commit your changes (\`git commit -m 'Add amazing feature'\`)
4. Push to the branch (\`git push origin feature/amazing-feature\`)
5. Open a Pull Request

Please ensure:
- Code follows existing style conventions
- All tests pass (when test suite exists)
- Documentation is updated for new features

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Credits

### Inspiration and Research
- **[DnG-Crafts/K2-RFID](https://github.com/DnG-Crafts/K2-RFID)** - Original reverse engineering of Creality CFS format
- Creality K2 Pro community for testing and feedback

### Libraries Used
- [MFRC522](https://github.com/miguelbalboa/rfid) - RFID communication
- [THiNX AESLib](https://github.com/suculent/thinx-aes-lib) - ESP32-compatible AES encryption
- ESP32 Arduino Core - Espressif Systems

## 📞 Support

- **Issues:** [GitHub Issues](https://github.com/yourusername/creality-cfs-programmer/issues)
- **Discussions:** [GitHub Discussions](https://github.com/yourusername/creality-cfs-programmer/discussions)
- **Email:** your.email@example.com

## 🗺️ Roadmap

### Phase 1: Reader (Current)
- [x] Basic tag reading
- [x] AES decryption
- [x] Tag parsing and display
- [ ] Multiple tag format support

### Phase 2: Writer
- [ ] Tag writing functionality
- [ ] Dual-tag workflow
- [ ] Write verification
- [ ] Backup/restore tags

### Phase 3: macOS Integration
- [ ] SwiftUI application
- [ ] BLE communication
- [ ] Filament database
- [ ] K2 Pro SSH integration
- [ ] Creality Print 6 sync

### Phase 4: Advanced Features
- [ ] Tag cloning/duplication
- [ ] Batch tag programming
- [ ] Custom material profiles
- [ ] Web interface (optional)
- [ ] Mobile app (iOS/Android)

---

**Made with ❤️ for the 3D printing community**

*If this project helped you, please consider giving it a ⭐️ on GitHub!*
