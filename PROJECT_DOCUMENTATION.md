# CFS Programmer - Complete Project Documentation

**Version:** 1.3.0  
**Last Updated:** December 26, 2025  
**Platform:** macOS  
**Hardware:** Creality K2 Pro 3D Printer + ESP32-S3 RFID Reader/Writer

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Database Structure](#database-structure)
4. [Material Data Model](#material-data-model)
5. [RFID Tag Format](#rfid-tag-format)
6. [Sync System](#sync-system)
7. [Features Reference](#features-reference)
8. [Versioning System](#versioning-system)
9. [Development Roadmap](#development-roadmap)

---

## Project Overview

### What is CFS Programmer?

CFS Programmer is a macOS application designed to manage, program, and sync filament material profiles for the Creality K2 Pro 3D printer's Custom Filament System (CFS). It provides a complete material database with RFID tag programming capabilities.

### Problem It Solves

1. **Limited Official Materials**: Creality only provides tags for their own filament
2. **No Custom Material Support**: Third-party filaments can't be used with CFS without custom tags
3. **Manual Configuration**: No easy way to manage material profiles across devices
4. **Profile Sharing**: Difficult to share proven material settings between users

### Core Components

- **Material Database Manager**: Store and organize filament profiles
- **RFID Tag Programmer**: Write custom tags for any filament
- **Bluetooth Device Manager**: Communicate with ESP32-S3 programmer
- **Sync Engine**: Keep profiles synchronized across CrealityPrint 6 and printer
- **Import/Export System**: Share profiles with community

---

## Architecture

### Technology Stack

**Frontend:**
- SwiftUI (macOS native UI)
- Combine (reactive data flow)
- CoreBluetooth (BLE communication)

**Backend:**
- Local JSON database
- File-based storage with automatic backups
- iCloud sync (optional)

**Hardware:**
- ESP32-S3 DevKit C-1
- PN532/RC522 NFC/RFID reader
- Mifare Classic 1K tags (ISO 14443-4)

### Project Structure

```
CFS Programmer/
├── Models/
│   ├── Material.swift          # FilamentMaterial struct
│   ├── Brand.swift              # Brand struct
│   ├── Color.swift              # Color utilities
│   ├── MaterialDatabase.swift   # Main database class
│   ├── SpoolInstance.swift      # Physical spool tracking
│   ├── AdvancedSettings.swift   # kvParam structure
│   └── DatabaseSettings.swift   # App settings
├── Managers/
│   └── DatabaseManager.swift    # All database operations
├── Utilities/
│   └── LengthCalculator.swift   # Weight-to-length conversion
├── Views/
│   └── ContentView.swift        # Main UI
└── Resources/
    └── materials.json           # Database file
```

---

## Database Structure

### Overview

The database is a single JSON file stored at:
```
~/Library/Application Support/CFS-Programmer/materials.json
```

### Schema

```json
{
  "version": "1.1.0",
  "lastUpdated": "2025-12-26T10:00:00Z",
  "databaseId": "cfs-programmer-materials",
  
  "materials": [ /* Array of FilamentMaterial */ ],
  "brands": [ /* Array of Brand */ ],
  "spoolInstances": [ /* Array of SpoolInstance */ ],
  
  "brandIdCounter": 1,
  "materialIdCounters": {
    "0276": 10,
    "F001": 5
  },
  "usedSerials": ["123456", "789012"],
  
  "densityStandards": {
    "PLA": 1.24,
    "PETG": 1.27,
    "ABS": 1.04,
    "TPU": 1.21
  },
  
  "settings": { /* DatabaseSettings */ },
  "backupInfo": { /* BackupInfo */ },
  "syncStatus": { /* SyncStatus */ }
}
```

### Database Fields

| Field | Type | Description |
|-------|------|-------------|
| `version` | String | Database schema version |
| `lastUpdated` | Date | Last modification timestamp |
| `databaseId` | String | Unique database identifier |
| `materials` | Array | All material profiles |
| `brands` | Array | All brands (official + custom) |
| `spoolInstances` | Array | Physical spools tracked |
| `brandIdCounter` | Int | Next available custom brand ID |
| `materialIdCounters` | Dict | Per-brand material counters |
| `usedSerials` | Array | Prevent duplicate serials |
| `densityStandards` | Dict | Material type densities |
| `settings` | Object | App configuration |
| `backupInfo` | Object | Backup metadata |
| `syncStatus` | Object | Sync state tracking |

---

## Material Data Model

### FilamentMaterial Structure

```swift
struct FilamentMaterial: Codable, Identifiable {
    // MARK: - Identity
    var id: String              // "F001-0001" (BrandID-MaterialNumber)
    var uuid: UUID              // Unique identifier
    var createdDate: Date
    var modifiedDate: Date
    
    // MARK: - Brand Association
    var brandId: String         // Links to Brand.id
    var brandName: String       // Cached for display
    var isCustom: Bool          // true for user-created
    
    // MARK: - Template Info
    var baseId: String          // "GFSA04" (Generic Filament Slicer A 0.4mm)
    var inherits: String        // "Generic PLA @Creality K2 Pro 0.4 nozzle"
    var templateSource: String  // "Generic PLA"
    var clonedFrom: String?     // Material ID if cloned
    
    // MARK: - Basic Info
    var name: String            // "SUNLU Transparent PLA"
    var materialType: String    // "PLA", "PETG", "ABS", etc.
    var materialSubtype: String // "Basic", "Silk", "Glow", "Matte"
    
    // MARK: - Color
    var colors: [MaterialColor] // Supports single/dual/gradient
    var colorType: ColorType    // .single, .dual, .gradient
    
    // MARK: - Physical Properties
    var density: Double         // g/cm³ (e.g., 1.24 for PLA)
    var densitySource: DensitySource // .standard or .custom
    var diameter: Double        // mm (typically 1.75 or 2.85)
    var weightOptions: [WeightOption] // Pre-calculated lengths
    
    // MARK: - Cost Tracking
    var purchaseInfo: PurchaseInfo
    
    // MARK: - Print Settings
    var temperatures: TemperatureSettings
    var speeds: SpeedSettings
    var kvParam: AdvancedSettings // 80+ parameters
    
    // MARK: - External References
    var externalRefs: ExternalReferences
    
    // MARK: - Sync Status
    var sync: MaterialSyncStatus
    
    // MARK: - Metadata
    var notes: String
    var tags: [String]
    var favorite: Bool
    var location: String        // Storage location
}
```

### Sub-Structures

#### MaterialColor
```swift
struct MaterialColor: Codable {
    var id: UUID
    var hex: String             // "FF0000"
    var name: String            // "Red" or auto-generated
    var isPrimary: Bool         // For CP6/printer sync
    var autoGeneratedName: Bool
}
```

#### WeightOption
```swift
struct WeightOption: Codable {
    var id: UUID
    var grams: Int              // 1000, 750, 500, 250
    var lengthMeters: Int       // Calculated from density
    var lengthHex: String       // "0330" for 1kg PLA
    var isDefault: Bool
}
```

#### TemperatureSettings
```swift
struct TemperatureSettings: Codable {
    var nozzle: NozzleTemp      // min, max, default, initialLayer
    var bed: BedTemp            // default, initialLayer
    var chamber: ChamberTemp    // default, required
}
```

#### AdvancedSettings (kvParam)
```swift
struct AdvancedSettings: Codable {
    var temperatures: TemperatureParams    // 9 settings
    var speeds: SpeedParams                // 14 settings
    var flow: FlowParams                   // 9 settings
    var cooling: CoolingParams             // 7 settings
    var retraction: RetractionParams       // 6 settings
    var support: SupportParams             // 7 settings
    var quality: QualityParams             // 7 settings
    var other: [String: String]            // Uncategorized
}
```

**Total Parameters**: 80+ settings covering every aspect of print configuration

### Brand Structure

```swift
struct Brand: Codable, Identifiable {
    var id: String              // "0276" (Creality) or "F001" (custom)
    var name: String            // "Creality", "SUNLU", etc.
    var isCustom: Bool          // true if user-created
    var isOfficial: Bool        // true for Creality
    var createdDate: Date
    var notes: String
}
```

**ID Format:**
- **Official Creality**: `"0276"` (hardcoded)
- **Custom Brands**: `"F001"` to `"F999"` (auto-generated)

### SpoolInstance Structure

```swift
struct SpoolInstance: Codable, Identifiable {
    var id: UUID
    var serial: String          // 6-digit unique serial
    var materialId: String      // Links to FilamentMaterial
    var weightGrams: Int        // Original weight
    var writeDate: Date         // When tag was written
    var location: String        // Storage location
    var notes: String
    var isActive: Bool          // In use vs depleted
    var remainingWeight: Int?   // Optional tracking
}
```

---

## RFID Tag Format

### Creality CFS Tag Structure

Tags are Mifare Classic 1K (1KB total, 16 sectors × 4 blocks)

**Block Layout:**

```
Block 0: Manufacturer Data (read-only)
Block 1-2: Unused
Block 3: Sector Trailer (keys)

Block 4: Date Code + Vendor ID (8 bytes)
Block 5: Material ID + Color (8 bytes)
Block 6: Length + Serial (8 bytes)
Block 7: Encryption Key (16 bytes - spans 2 blocks)
```

### Data Format

**Complete Tag Data (48 bytes hex):**
```
AB124 0276 A2 101001 FF0000 0330 123456 0000000000000000
└───┘ └──┘ │  └────┘ └────┘ └──┘ └────┘ └──────────────┘
Date  Vend │  Film   Color   Len  Serial    Reserved
      or   │   ID
     Brand │
          Type
```

#### Field Breakdown

| Field | Bytes | Format | Description | Example |
|-------|-------|--------|-------------|---------|
| Date Code | 5 | ASCII | `AB` + month(hex) + day(hex) + year(00-99) | `AB124` = Dec 18, 2024 |
| Vendor ID | 4 | Hex | Brand identifier | `0276` = Creality |
| Type | 2 | Hex | Material category | `A2` = PLA |
| Filament ID | 6 | Hex | Material type code | `101001` = Standard PLA |
| Color | 6 | Hex | RGB color code | `FF0000` = Red |
| Length | 4 | Hex | Filament length in meters | `0330` = 816m (1kg PLA) |
| Serial | 6 | Dec | Unique identifier | `123456` |
| Reserved | 16 | Hex | Future use | All zeros |

### Encryption

**Standard Tags**: No encryption (default Key A/B: `FFFFFFFFFFFF`)

**Encrypted Tags** (Advanced):
1. Generate AES key from UID
2. Encrypt blocks 4-6 with AES-128
3. Store encryption key in block 7
4. Write encrypted data to blocks 4-6

---

## Sync System

### Three-Way Sync Architecture

```
┌─────────────────────┐
│  CFS Programmer DB  │ ← Master database
└──────────┬──────────┘
           │
           ├─────────────────┐
           │                 │
           ▼                 ▼
┌──────────────────┐  ┌─────────────────┐
│ CrealityPrint 6  │  │  K2 Pro Printer │
│   (Local Files)  │  │  (SSH/Network)  │
└──────────────────┘  └─────────────────┘
```

### 1. CrealityPrint 6 Sync

**Target Directory:**
```
~/Library/Application Support/Creality/Creality Print/6.0/user/[USER_ID]/filament/
```

**File Format:**
```
BRAND_NAME MATERIAL_NAME @MACHINE_PROFILE.json
```

**Example:**
```
SUNLU Transparent PLA @Creality K2 Pro 0.4 nozzle.json
```

**CP6 JSON Structure:**
```json
{
  "version": "01.07.00.60",
  "name": "SUNLU Transparent PLA @Creality K2 Pro 0.4 nozzle",
  "from": "system",
  "instantiation": "true",
  "inherits": "Generic PLA",
  "filament_id": ["101001"],
  "filament_colour": [["#FF0000"]],
  "filament_type": ["PLA"],
  "nozzle_temperature": ["210"],
  "nozzle_temperature_initial_layer": ["215"],
  "bed_temperature": ["60"],
  "filament_density": ["1.24"],
  "filament_diameter": ["1.75"],
  "filament_flow_ratio": ["1.0"],
  "filament_max_volumetric_speed": ["21"]
}
```

**Key Transformations:**
- All values become string arrays: `"210"` → `["210"]`
- Colors become nested arrays: `"#FF0000"` → `[["#FF0000"]]`
- kvParam flattened to top level
- Inherit chain preserved

**Sync Process:**
1. Select material to sync
2. Generate CP6 filename
3. Flatten kvParam structure
4. Convert all values to arrays
5. Write to CP6 directory
6. Update `sync.cp6Synced = true`
7. Record timestamp

### 2. Printer Sync (SSH)

**Connection Method:** SSH over network

**Default Settings:**
```
Host: [printer-ip]
Port: 22
Username: root
Auth: Password or SSH key
```

**Target Path:**
```
/mnt/UDISK/creality/userdata/box/material_database.json
```

**Printer JSON Format:**
```json
{
  "filament_id": "101001",
  "filament_type": "PLA",
  "nozzle_temperature": [190, 230],
  "bed_temperature": [60, 85],
  "filament_colour": "#FF0000",
  "filament_density": 1.24
}
```

**Sync Process:**
1. Establish SSH connection
2. Backup existing material_database.json
3. Convert CFS format → Printer format
4. Upload via SCP
5. Verify upload
6. Update `sync.printerSynced = true`
7. Record timestamp

### 3. Tag Sync (RFID)

**Physical Tag Programming:**

**Workflow:**
1. Select material from database
2. Choose weight option (1kg/750g/500g/250g)
3. Generate or input serial number
4. Calculate length from weight + density
5. Format tag data (48 bytes)
6. Connect to programmer via Bluetooth
7. Write Block 4: Date + Vendor + Type + FilmID
8. Write Block 5: Color
9. Write Block 6: Length + Serial
10. Optional: Write Block 7 with encryption key
11. Create SpoolInstance record
12. Update `sync.printerSynced = true`

**Dual Tag Set:**
- **Front Tag (A)**: Full data
- **Back Tag (B)**: Identical data, same serial
- Both must match for K2 Pro to accept

---

## Features Reference

### 1. Material Management

#### Create Material
**Purpose:** Add new filament profile to database

**Process:**
1. Select brand (or create new)
2. Enter material name
3. Choose material type (PLA, PETG, etc.)
4. Select template to clone settings from
5. Optionally customize colors
6. Save to database

**Generated Data:**
- Unique ID: `BRAND_ID-####`
- UUID for internal tracking
- Weight options auto-calculated
- Default settings from template
- Creation timestamp

#### Edit Material
**Purpose:** Modify existing material profile

**Editable Fields:**
- Basic: name, type, subtype
- Colors: add/remove/reorder colors
- Temperatures: nozzle, bed, chamber
- Speeds: print, layer, wall speeds
- Advanced: all 80+ kvParam settings
- Cost: price per kg, purchase info
- Notes: storage location, tags

**Validation:**
- Required fields checked
- Density range: 0.5-2.5 g/cm³
- Temperature ranges enforced
- Speed limits validated

#### Delete Material
**Purpose:** Remove material from database

**Safety Checks:**
- Confirm deletion dialog
- Check for linked SpoolInstances
- Warn if synced to CP6/printer
- Cannot be undone

#### Clone Material
**Purpose:** Duplicate material with variations

**Use Cases:**
- Same brand, different color
- Same filament, different nozzle size
- Experimental setting tweaks

**Process:**
1. Select material to clone
2. Enter new name
3. All settings copied
4. `clonedFrom` field populated
5. New unique ID assigned

### 2. Brand Management

#### Create Brand
**Purpose:** Add third-party filament manufacturer

**Auto-Generated:**
- Brand ID: F001-F999
- Creation date
- Material counter initialized

**Manual Input:**
- Brand name (e.g., "SUNLU", "eSUN")
- Notes (website, quality notes)

#### Edit Brand
**Purpose:** Update brand information

**Editable:**
- Name
- Notes

**Read-Only:**
- ID (cannot change)
- Material count (computed)
- Official status (Creality only)

#### Delete Brand
**Purpose:** Remove unused brand

**Restrictions:**
- Cannot delete if materials exist
- Cannot delete official brands
- Must manually delete materials first

### 3. Color Management

#### Color Types

**Single Color:**
- Most common
- One hex value
- Auto-named or manual

**Dual Color:**
- Silk, gradient filaments
- Two hex values
- Primary/secondary designation

**Gradient:**
- Color-changing filaments
- Multiple transition colors
- Visual representation

#### Color Utilities

**Auto-Naming:**
- 60+ named colors in database
- Closest match algorithm
- Descriptive names generated
- Examples: "Dark Orange Tint", "Sky Blue"

**Custom Names:**
- Override auto-generated
- User-defined labels
- Maintain hex value

### 4. RFID Tag Operations

#### Read Tag
**Purpose:** Decode existing CFS tag

**Process:**
1. Connect to programmer
2. Click "Read Tag"
3. Place tag on reader
4. Data decoded and displayed
5. Match to database material (if exists)

**Displayed Info:**
- Material type
- Vendor/brand
- Color (hex + visual)
- Length remaining
- Serial number
- Blank tag detection
- Non-Creality tag warning

#### Write Tag (Single)
**Purpose:** Program one RFID tag

**Workflow:**
1. Select material
2. Choose weight option
3. Pick color (if multi-color)
4. Generate or enter serial
5. Review preview
6. Click "Write Tag"
7. Place tag on reader
8. Confirmation on success

**Validation:**
- Material selected
- Valid serial (6 digits)
- Weight option chosen
- Bluetooth connected

#### Write Dual Tag Set
**Purpose:** Program matching front/back tags

**Why Dual Tags?**
- K2 Pro requires both for verification
- Redundancy for reliability
- Both must have same serial

**Process:**
1. Configure material (as above)
2. Click "Write Both Tags"
3. Write Tag 1: Place first tag → Success
4. Write Tag 2: Place second tag → Success
5. Both tags stored with same serial
6. SpoolInstance created

### 5. Spool Tracking

#### Create Spool Instance
**Purpose:** Track physical filament spool

**Recorded:**
- Serial number (from tag)
- Material ID (links to profile)
- Original weight
- Write date
- Storage location
- Notes
- Active status

**Use Cases:**
- Inventory management
- Usage tracking
- Location tracking
- Warranty tracking

#### View Spools
**Purpose:** Browse all tracked spools

**Displayed:**
- Material name/brand
- Serial number
- Weight (original/remaining)
- Location
- Status (active/depleted)
- Age (days since write)

**Sorting:**
- By date (newest first)
- By material
- By location
- By status

#### Update Spool
**Purpose:** Modify spool information

**Editable:**
- Location (moved to storage)
- Notes (print quality observed)
- Remaining weight (manual input)
- Active status (mark depleted)

#### Delete Spool
**Purpose:** Remove spool from tracking

**When to Use:**
- Spool completely used
- Tag damaged/unreadable
- Cleaning up old records

**Note:** Deleting spool doesn't remove material profile

### 6. Settings Management

#### Database Settings

**Machine Profile:**
- Default: "Creality K2 Pro 0.4 nozzle"
- Used in CP6 filenames
- Affects template selection

**Default Values:**
- Filament diameter: 1.75mm or 2.85mm
- Currency: USD, EUR, GBP, etc.
- Decimal separator: . or ,

**Backup Settings:**
- Enable iCloud sync: true/false
- Daily snapshots: true/false
- Keep last X backups: 30 days default

#### SSH Settings

**Printer Connection:**
- Host: Printer IP address
- Port: 22 (default SSH)
- Username: root (default)
- Authentication: password or key
- Remote path: `/mnt/UDISK/creality/...`

**Options:**
- Backup before sync: recommended
- Auto-reconnect: true/false
- Connection timeout: seconds

### 7. Import/Export

#### Import from 3dprintprofiles.com
**Purpose:** Migrate existing spool data

**File Format:** `my-spools.json`

**Process:**
1. Upload JSON file
2. Parse spool data
3. Extract materials, brands, colors
4. Auto-create missing brands
5. Match or create material profiles
6. Import external references
7. Create SpoolInstances

**Data Mapped:**
- Brand → Brand.name
- Material → FilamentMaterial
- Color → MaterialColor
- Weight → weightGrams
- Location → location
- Notes → notes
- URLs → externalRefs

#### Export Database
**Purpose:** Backup or share profiles

**Format:** Complete JSON dump

**Includes:**
- All materials
- All brands
- All spools
- Settings
- Metadata

**Use Cases:**
- Backup before major changes
- Share profiles with community
- Transfer to another computer
- Archive old configurations

#### Import Database
**Purpose:** Restore from backup

**Options:**
- **Replace:** Overwrite current database
- **Merge:** Combine with existing (future)

**Safety:**
- Auto-backup before import
- Validation checks
- Rollback on error

### 8. Backup & Recovery

#### Automatic Backups

**Pre-Save Backup:**
- Before every save
- File: `materials.backup.json`
- Instant recovery point

**Daily Snapshots:**
- Timestamp-based filenames
- Example: `materials_2025-12-26_103045.json`
- Stored in `backups/` directory
- Auto-cleanup after 30 days

**iCloud Sync (Optional):**
- Real-time sync to iCloud Drive
- Accessible on multiple devices
- Conflict resolution built-in

#### Manual Backups

**Create Snapshot:**
- Click "Create Backup"
- Timestamped file created
- Stored in backups directory
- Not auto-deleted

**Export Complete:**
- Full database export
- Save anywhere
- Useful before major changes

#### Recovery Options

**1. Auto-Recovery:**
- If `materials.json` corrupted
- Automatically loads `.backup.json`
- User notified

**2. Snapshot Restore:**
- Browse available backups
- Select by date/time
- Preview before restore
- Creates safety backup first

**3. Manual Import:**
- Import any valid JSON
- Full validation
- Merge or replace options

**4. iCloud Restore:**
- Download from iCloud
- Resolve conflicts if needed
- Sync to local

---

## Versioning System

### Semantic Versioning

**Format:** `MAJOR.MINOR.PATCH`

**Example:** `1.3.0`

### Version Components

#### MAJOR (1.x.x)
**When to Increment:**
- Breaking database schema changes
- Incompatible API changes
- Major architecture overhaul
- Removal of core features

**Example Changes:**
- Database format complete redesign
- RFID tag format changes
- Bluetooth protocol breaking changes

**Migration Required:** YES

#### MINOR (x.3.x)
**When to Increment:**
- New features added
- New database fields (backward compatible)
- UI improvements
- Enhanced functionality

**Example Changes:**
- Add gradient color support
- Add encryption for tags
- Add iCloud sync
- Add SSH printer sync

**Migration Required:** NO (usually)

#### PATCH (x.x.0)
**When to Increment:**
- Bug fixes
- Performance improvements
- UI polish
- Documentation updates
- Security patches

**Example Changes:**
- Fix calculation errors
- Fix UI rendering issues
- Fix memory leaks
- Update dependencies

**Migration Required:** NO

### Version History

#### 1.0.0 - Initial Release
- Basic material database
- Brand management
- Simple tag writing
- Bluetooth communication

#### 1.1.0 - Color Enhancement
- Multi-color support (single/dual/gradient)
- Auto color naming
- Enhanced color picker
- Color database (60+ names)

#### 1.2.0 - OTA Updates
- WiFi configuration
- GitHub release checking
- OTA firmware updates
- Progress tracking

#### 1.3.0 - Database Overhaul
- Modular architecture
- DatabaseManager singleton
- FilamentMaterial renaming
- Enhanced spool tracking
- Backup system improvements

### Database Schema Versions

**Format:** Separate from app version

**Current:** `1.1.0`

**Migration Path:**
```
1.0.0 → 1.1.0: Add colors array, colorType field
1.1.0 → 1.2.0: Add sync status, backup info
```

**Compatibility:**
- App 1.3.0 reads DB 1.0.0+ (backward compatible)
- DB 1.1.0 requires App 1.2.0+ (forward compatible)

### Firmware Versioning

**ESP32-S3 Firmware:** Separate versioning

**Format:** `v1.2.0` or `1.2.OTA`

**Features by Version:**

**v1.0:**
- Basic RFID read/write
- Bluetooth serial
- Single tag support

**v1.1:**
- Dual tag support
- Tag verification
- Error handling

**v1.2.OTA:**
- WiFi configuration
- OTA update support
- GitHub integration
- Automatic updates

**Checking:** App queries firmware on connect

**Updating:** Via OTA (if WiFi configured) or USB

### Update Mechanism

#### App Updates

**Distribution:**
- GitHub Releases
- Direct download
- Manual install

**Process:**
1. User downloads new .app
2. Replace old version
3. Launch new version
4. Auto-migration if needed

#### Firmware Updates

**Over-The-Air (OTA):**
1. Configure WiFi on device
2. App checks GitHub releases
3. Compare versions
4. Download .bin file
5. Send OTA command
6. Device downloads firmware
7. Device reboots
8. Auto-reconnect

**USB Update:**
1. Download firmware .bin
2. Connect via USB
3. Use esptool or Arduino IDE
4. Flash new firmware

#### Database Migration

**Automatic:**
```swift
if databaseVersion < currentVersion {
    migrate(from: databaseVersion, to: currentVersion)
}
```

**Migration Steps:**
1. Backup current database
2. Load old format
3. Transform data structure
4. Add new fields with defaults
5. Save in new format
6. Update version field

**Rollback:**
- Keep old backup
- User can revert if issues
- Documented process

---

## Development Roadmap

### Phase 1: Core Functionality ✅ COMPLETE
- ✅ Material database structure
- ✅ Brand management
- ✅ Basic RFID operations
- ✅ Bluetooth communication
- ✅ Local JSON storage

### Phase 2: Enhanced Features ✅ COMPLETE
- ✅ Multi-color support
- ✅ Advanced settings (kvParam)
- ✅ Weight calculations
- ✅ Spool tracking
- ✅ Auto backups

### Phase 3: Sync System 🚧 IN PROGRESS
- ✅ DatabaseManager architecture
- ✅ Backup system
- ⏳ CrealityPrint 6 sync
- ⏳ SSH printer sync
- ⏳ Import from 3dprintprofiles

### Phase 4: User Interface 📋 PLANNED
- ⏳ SwiftUI views for all features
- ⏳ Material editor UI
- ⏳ Advanced settings UI
- ⏳ Spool management UI
- ⏳ Settings panel

### Phase 5: Polish 📋 PLANNED
- ⏳ Error handling UI
- ⏳ Help system
- ⏳ Tooltips
- ⏳ Keyboard shortcuts
- ⏳ Dark mode refinements

### Phase 6: Advanced Features 💭 FUTURE
- 💭 iCloud sync
- 💭 Community profile sharing
- 💭 Profile marketplace
- 💭 Print statistics
- 💭 Material recommendations
- 💭 Auto-ordering reminders

### Phase 7: Hardware Support 💭 FUTURE
- 💭 Support other RFID readers
- 💭 USB serial support
- 💭 Network-attached readers
- 💭 Multi-device management

---

## Troubleshooting

### Common Issues

#### "Cannot Connect to Device"
**Causes:**
- Bluetooth disabled
- Device not powered
- Wrong firmware version
- Another app connected

**Solutions:**
1. Check Bluetooth is on
2. Power cycle ESP32
3. Close other serial apps
4. Reset Bluetooth module

#### "Tag Write Failed"
**Causes:**
- Tag not on reader
- Wrong tag type
- Tag locked/encrypted
- Reader malfunction

**Solutions:**
1. Position tag correctly
2. Use Mifare Classic 1K tags
3. Check tag keys (default FF...)
4. Test with different tag

#### "Database Corrupted"
**Causes:**
- Incomplete write
- Disk full
- Power loss during save
- File permission issues

**Solutions:**
1. Auto-recovery loads backup
2. Restore from snapshot
3. Import exported backup
4. Check disk space

#### "CP6 Sync Not Working"
**Causes:**
- CP6 not installed
- Wrong user directory
- Permission denied
- Path changed

**Solutions:**
1. Verify CP6 installation
2. Check user directory path
3. Grant file access
4. Update path in settings

#### "Printer Sync Failed"
**Causes:**
- Network unreachable
- Wrong credentials
- SSH disabled on printer
- Firewall blocking

**Solutions:**
1. Ping printer IP
2. Verify SSH credentials
3. Enable SSH in printer settings
4. Check firewall rules

---

## API Reference

### DatabaseManager

#### Material Operations
```swift
// Get all materials
func getAllMaterials() -> [FilamentMaterial]

// Get materials by brand
func getMaterials(for brandId: String) -> [FilamentMaterial]

// Get single material
func getMaterial(id: String) -> FilamentMaterial?

// Search materials
func searchMaterials(query: String) -> [FilamentMaterial]

// Create new material
func createMaterial(
    brandId: String,
    name: String,
    materialType: String,
    templateSource: String,
    cloneSettings: FilamentMaterial?
) -> FilamentMaterial

// Update material
func updateMaterial(_ material: FilamentMaterial)

// Delete material
func deleteMaterial(id: String)
```

#### Brand Operations
```swift
// Get all brands
func getAllBrands() -> [Brand]

// Get single brand
func getBrand(id: String) -> Brand?

// Create brand
func createBrand(name: String, notes: String) -> Brand

// Update brand
func updateBrand(_ brand: Brand)

// Delete brand (with safety checks)
func deleteBrand(id: String) -> Result<Void, BrandDeletionError>

// Get material count
func getMaterialCount(for brandId: String) -> Int
```

#### Spool Operations
```swift
// Create spool instance
func createSpoolInstance(
    materialId: String,
    serial: String,
    weightGrams: Int,
    location: String,
    notes: String
) -> SpoolInstance

// Get all spools
func getAllSpoolInstances() -> [SpoolInstance]

// Get spools for material
func getSpoolInstances(for materialId: String) -> [SpoolInstance]

// Get spool by serial
func getSpoolInstance(serial: String) -> SpoolInstance?

// Update spool
func updateSpoolInstance(_ spool: SpoolInstance)

// Delete spool
func deleteSpoolInstance(serial: String)
```

#### Utility Operations
```swift
// Generate unique serial
func generateUniqueSerial() -> String

// Calculate weight options
func calculateWeightOptions(
    density: Double,
    diameter: Double
) -> [WeightOption]

// Get density standard
func getDensityStandard(for materialType: String) -> Double

// Set custom density
func setDensityStandard(
    for materialType: String,
    density: Double
)
```

#### Backup Operations
```swift
// Create timestamped backup
func createDailySnapshot() -> URL?

// List available backups
func listBackups() -> [URL]

// Restore from backup
func restoreFromBackup(url: URL) -> Bool

// Update settings
func updateSettings(_ settings: DatabaseSettings)
```

---

## Contributing

### Code Style

**Swift:**
- SwiftLint rules enforced
- 4-space indentation
- Descriptive variable names
- Comments for complex logic

**Documentation:**
- All public functions documented
- Usage examples provided
- Edge cases noted

### Testing

**Unit Tests:**
- Model encoding/decoding
- Calculation accuracy
- ID generation uniqueness

**Integration Tests:**
- Database save/load
- Bluetooth communication
- File operations

**UI Tests:**
- Critical workflows
- Error handling
- Edge cases

### Pull Requests

**Required:**
1. Description of changes
2. Test coverage
3. Documentation updates
4. Version bump (if needed)

**Review Process:**
1. Automated tests pass
2. Code review approval
3. Documentation reviewed
4. Merge to develop
5. Release from main

---

## License

MIT License - See LICENSE file

---

## Support

**GitHub Issues:** https://github.com/yourusername/cfs-programmer/issues

**Documentation:** https://github.com/yourusername/cfs-programmer/wiki

**Community:** Discord/Reddit/Forum links

---

## Acknowledgments

- **Creality** for the K2 Pro CFS system
- **ESP32 Community** for Arduino libraries
- **MFRC522 Library** for RFID support
- **3dprintprofiles.com** for spool tracking inspiration

---

**Last Updated:** December 26, 2025  
**Document Version:** 1.0  
**App Version:** 1.3.0
