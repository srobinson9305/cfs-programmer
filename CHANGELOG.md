# Changelog

All notable changes to the CFS Programmer firmware will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Feature descriptions here

### Changed
- Changes to existing features

### Fixed
- Bug fixes

## [1.2.0] - 2025-12-21

### Added
- OTA firmware update support via WiFi
- Firmware versioning system with automatic version reporting over BLE
- WiFi configuration over BLE (WIFI_CONFIG command)
- GitHub releases integration for automatic update checking
- CHECK_UPDATE command to query GitHub API for new versions
- OTA_UPDATE command to download and install firmware from URL
- Blank tag detection (all zeros in data blocks)
- Magenta LED color for OTA update in progress
- Build date/time tracking in firmware

### Changed
- Improved BLE protocol with version handshake on connect
- Enhanced error messages for better debugging
- Updated serial output formatting for better readability

### Fixed
- None in this release

## [1.1.0] - 2025-12-20

### Added
- Authentication retry logic with up to 3 attempts per operation
- Tag re-selection between retry attempts for better reliability
- Blank tag detection and reporting
- Improved serial output with formatted boxes and emojis

### Changed
- Authentication now tries multiple key combinations automatically
- Better error handling for authentication failures

### Fixed
- Timing issues with PN532 authentication (requires library patch)
- Intermittent authentication failures on valid tags

## [1.0.0] - 2025-12-19

### Added
- Initial release
- Read Creality CFS tags via PN532 NFC reader
- Decrypt tag data using AES-128
- UID-based MIFARE key generation
- BLE interface for Mac app communication
- OLED display support (SH1106 128x64)
- WS2812B RGB LED status indicators
- Support for MIFARE Classic 1K tags

### Supported Commands
- READ - Read and decode CFS tag
- CANCEL - Cancel current operation
- GET_VERSION - Request firmware version

---

## Version Number Guidelines

- **MAJOR** version: Breaking changes to BLE protocol or hardware requirements
- **MINOR** version: New features, backward compatible
- **PATCH** version: Bug fixes only

## Release Process

1. Update version in firmware code: `#define FIRMWARE_VERSION "X.Y.Z"`
2. Update this CHANGELOG.md with changes
3. Commit changes: `git commit -m "Release vX.Y.Z"`
4. Create tag: `git tag -a fw-vX.Y.Z -m "Version X.Y.Z"`
5. Push: `git push origin main --tags`
6. GitHub Actions automatically builds and creates release
