# Library Dependencies

This document explains how the GitHub Actions workflow handles library dependencies.

## Current Approach (v1.2.0+)

The workflow uses **automatic library detection** combined with **explicit fallback** installation.

### How It Works

1. **Extraction Phase**
   ```bash
   # Scans the .ino file for #include statements
   grep -E "^#include <" CFS_Handheld_v1.2_OTA.ino
   ```

2. **Mapping Phase**
   ```bash
   # Maps header names to library names
   Adafruit_PN532.h â†’ "Adafruit PN532@1.3.1"
   U8g2lib.h â†’ "U8g2@2.35.9"
   etc.
   ```

3. **Installation Phase**
   ```bash
   # Installs each detected library
   arduino-cli lib install "Library Name@Version"
   ```

4. **Fallback Phase**
   ```bash
   # Explicitly installs known critical libraries
   # This ensures the build works even if detection fails
   ```

## Adding New Libraries

### Method 1: Automatic Detection (Preferred)

Just add the library to your firmware:

```cpp
#include <NewLibrary.h>
```

Then update the workflow's library map:

```yaml
declare -A LIB_MAP=(
  ["NewLibrary"]="New Library Package@1.0.0"
  # ... existing mappings
)
```

### Method 2: Explicit Installation

Add to the fallback section:

```yaml
- name: Install required libraries
  run: |
    # ... existing libraries
    arduino-cli lib install "New Library@1.0.0"
```

## Current Library Requirements

| Header File | Library Package | Version | Notes |
|-------------|----------------|---------|-------|
| `Adafruit_PN532.h` | Adafruit PN532 | 1.3.1 | Requires SLOWDOWN patch |
| `U8g2lib.h` | U8g2 | 2.35.9 | OLED driver |
| `Adafruit_NeoPixel.h` | Adafruit NeoPixel | 1.12.0 | WS2812 LED |
| `AESLib.h` | AESLib | 2.1.0 | Encryption |
| `ArduinoJson.h` | ArduinoJson | 6.21.3 | JSON parsing |
| `WiFi.h` | Built-in | - | ESP32 core |
| `HTTPClient.h` | Built-in | - | ESP32 core |
| `Update.h` | Built-in | - | ESP32 core |
| `BLEDevice.h` | Built-in | - | ESP32 core |
| `Wire.h` | Built-in | - | Arduino core |

## Built-in Libraries

These are included with the ESP32 core and don't need separate installation:

- `WiFi.h`
- `HTTPClient.h`
- `Update.h`
- `BLEDevice.h`, `BLEServer.h`, `BLEUtils.h`, `BLE2902.h`
- `Wire.h`

## Version Pinning

We pin library versions to ensure reproducible builds:

**Why?**
- Prevents breaking changes in minor updates
- Ensures GitHub Actions builds match local builds
- Makes debugging easier

**When to update?**
- Bug fixes in libraries
- New features needed
- Security updates

**How to update?**
1. Test new version locally
2. Update version in workflow
3. Update version in this doc
4. Create new firmware release

## Troubleshooting

### Library Not Found

**Symptom:** Build fails with "library not found"

**Solution:**
1. Add library to the `LIB_MAP` in workflow
2. Or add explicit installation in fallback section

### Version Conflict

**Symptom:** Build succeeds but firmware crashes

**Solution:**
1. Check library compatibility with ESP32 core v2.0.14
2. Pin to known-good version
3. Test thoroughly before release

### Library Name Mismatch

**Symptom:** Workflow can't find library by header name

**Solution:**

Some libraries have different package names than header names:

```bash
# Header: Adafruit_PN532.h
# Package: "Adafruit PN532"  (note the space, no underscore)
```

Add explicit mapping in `LIB_MAP`.

## Future Improvements

### Possible Enhancements

1. **Automatic library.properties parsing**
   - Read library requirements from sketch metadata
   - No manual mapping needed

2. **Dependency resolution**
   - Automatically install library dependencies
   - Handle version conflicts

3. **Library caching**
   - Cache libraries between workflow runs
   - Faster builds

4. **Version range support**
   - Allow `>=1.3.0,<2.0.0` style versioning
   - More flexibility in updates

## Testing Library Changes

Before pushing library updates:

```bash
# 1. Clean build locally
rm -rf build/
arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/CFS_Handheld_v1.2_OTA/

# 2. Test on real hardware
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:esp32s3 firmware/CFS_Handheld_v1.2_OTA/

# 3. Verify all features work
# - BLE connection
# - Tag reading
# - OTA updates

# 4. Create test release
git tag fw-v1.2.1-beta
git push origin fw-v1.2.1-beta

# 5. Verify GitHub Actions build
# Check: https://github.com/USERNAME/REPO/actions
```

## Support

If you encounter library issues:

1. Check the [GitHub Actions logs](../../actions)
2. Compare installed versions with this document
3. Test locally with same library versions
4. Open an issue with build log output
