//
//  Material.swift
//  CFS-Programmer
//
//  Created on 2025-12-24
//

import Foundation

struct FilamentMaterial: Codable, Identifiable {
    
    // MARK: - Identity
    var id: String                          // "F001-0001"
    var uuid: UUID = UUID()
    var createdDate: Date = Date()
    var modifiedDate: Date = Date()
    
    // MARK: - Brand Association
    var brandId: String                     // Links to Brand.id
    var brandName: String                   // Cached for display
    var isCustom: Bool = true
    
    // MARK: - Template Info
    var baseId: String                      // "GFSA04"
    var inherits: String                    // "Generic PLA @Creality K2 Pro 0.4 nozzle"
    var templateSource: String              // "Generic PLA"
    var clonedFrom: String?                 // Material ID if cloned
    
    // MARK: - Basic Info
    var name: String                        // "SUNLU Transparent PLA"
    var materialType: String                // "PLA", "PETG", etc.
    var materialSubtype: String = "Basic"   // "Basic", "Silk", "Glow", etc.
    
    // MARK: - Color
    var colors: [MaterialColor] = []        // Now imported from Color.swift
    var colorType: ColorType = .single      // Now imported from Color.swift
    
    // MARK: - Physical Properties
    var density: Double                     // g/cmÂ³
    var densitySource: DensitySource = .standard
    var diameter: Double = 1.75             // mm
    var weightOptions: [WeightOption] = []
    
    // MARK: - Cost
    var purchaseInfo: PurchaseInfo = PurchaseInfo()
    
    // MARK: - Temperatures
    var temperatures: TemperatureSettings = TemperatureSettings()
    
    // MARK: - Speeds
    var speeds: SpeedSettings = SpeedSettings()
    
    // MARK: - Advanced Settings
    var kvParam: AdvancedSettings = AdvancedSettings()
    
    // MARK: - External References
    var externalRefs: ExternalReferences = ExternalReferences()
    
    // MARK: - Sync Status
    var sync: MaterialSyncStatus = MaterialSyncStatus()
    
    // MARK: - Notes & Metadata
    var notes: String = ""
    var tags: [String] = []
    var favorite: Bool = false
    var location: String = ""
}

// MARK: - Supporting Enums

enum DensitySource: String, Codable {
    case standard
    case custom
}

// MARK: - Weight Option

struct WeightOption: Codable, Identifiable {
    var id: UUID = UUID()
    var grams: Int                      // 1000, 750, 500, 250
    var lengthMeters: Int               // Calculated
    var lengthHex: String               // For RFID tag
    var isDefault: Bool = false
}

// MARK: - Purchase Info

struct PurchaseInfo: Codable {
    var costPerKg: Double = 0.0
    var currency: String = "USD"
    var costPerMeter: Double = 0.0      // Auto-calculated
    var purchaseLink: String?
    var purchaseDate: Date?
}

// MARK: - Temperature Settings

struct TemperatureSettings: Codable {
    var nozzle: NozzleTemp = NozzleTemp()
    var bed: BedTemp = BedTemp()
    var chamber: ChamberTemp = ChamberTemp()
}

struct NozzleTemp: Codable {
    var min: Int = 190
    var max: Int = 230
    var defaultTemp: Int = 210
    var initialLayer: Int = 215
}

struct BedTemp: Codable {
    var defaultTemp: Int = 60
    var initialLayer: Int = 65
}

struct ChamberTemp: Codable {
    var defaultTemp: Int = 0
    var required: Bool = false
}

// MARK: - Speed Settings

struct SpeedSettings: Codable {
    var maxPrintSpeed: Int = 300        // mm/s
    var firstLayerSpeed: Int = 50       // mm/s
    var outerWallSpeed: Int = 150       // mm/s
    var innerWallSpeed: Int = 200       // mm/s
}

// MARK: - External References

struct ExternalReferences: Codable {
    var spooldbUrl: String?
    var filamentUrl: String?
    var importedFrom: String?           // "3dprintprofiles" or nil
}

// MARK: - Material Sync Status

struct MaterialSyncStatus: Codable {
    var cp6Synced: Bool = false
    var cp6LastSync: Date?
    var cp6Filename: String = ""
    var printerSynced: Bool = false
    var printerLastSync: Date?
    var syncConflicts: [String] = []
}

// MARK: - Material Extensions

extension FilamentMaterial {
    /// Get primary color for CP6/Printer sync
    var primaryColor: MaterialColor? {
        return colors.first { $0.isPrimary } ?? colors.first
    }
    
    /// Get display name for UI
    var displayName: String {
        return name
    }
    
    /// Get CP6 compatible filename
    func cp6Filename(machineProfile: String) -> String {
        return "\(brandName) \(name) @\(machineProfile)"
    }
    
    /// Check if material has been modified from template
    var isModified: Bool {
        return modifiedDate > createdDate
    }
}
