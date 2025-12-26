//
//  MaterialDatabase.swift
//  CFS Programmer
//
//  Created by Steven Robinson on 12/24/25.
//

import Foundation
import Combine

/// Main database containing all materials, brands, and tracking data
class MaterialDatabase: Codable, ObservableObject {
    
    // MARK: - Published Properties
    @Published var materials: [FilamentMaterial] = []
    @Published var brands: [Brand] = []
    @Published var settings: DatabaseSettings = DatabaseSettings()
    
    // MARK: - Metadata
    var version: String = "1.1.0"
    var lastUpdated: Date = Date()
    var databaseId: String = "cfs-programmer-materials"
    
    // MARK: - Tracking
    var brandIdCounter: Int = 1
    var materialIdCounters: [String: Int] = [:] // Brand ID -> Counter
    var usedSerials: [String] = []
    var spoolInstances: [SpoolInstance] = []
    
    // MARK: - Standards
    var densityStandards: [String: Double] = [
        "PLA": 1.24,
        "PLA+": 1.23,
        "PETG": 1.27,
        "ABS": 1.04,
        "TPU": 1.21,
        "Nylon": 1.14,
        "ASA": 1.07,
        "PC": 1.20,
        "HIPS": 1.04,
        "PVA": 1.23
    ]
    
    // MARK: - Backup Info
    var backupInfo: BackupInfo = BackupInfo()
    
    // MARK: - Sync Status
    var syncStatus: SyncStatus = SyncStatus()
    
    // MARK: - Initialization
    init() {
        // Initialize with default Creality brand
        let crealityBrand = Brand(
            id: "0276",
            name: "Creality",
            isCustom: false,
            isOfficial: true
        )
        brands.append(crealityBrand)
    }
    
    // MARK: - Codable
    enum CodingKeys: String, CodingKey {
        case version, lastUpdated, databaseId
        case materials, brands, settings
        case brandIdCounter, materialIdCounters
        case usedSerials, spoolInstances
        case densityStandards, backupInfo, syncStatus
    }
    
    required init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        
        version = try container.decode(String.self, forKey: .version)
        lastUpdated = try container.decode(Date.self, forKey: .lastUpdated)
        databaseId = try container.decode(String.self, forKey: .databaseId)
        
        materials = try container.decode([FilamentMaterial].self, forKey: .materials)
        brands = try container.decode([Brand].self, forKey: .brands)
        settings = try container.decode(DatabaseSettings.self, forKey: .settings)
        
        brandIdCounter = try container.decode(Int.self, forKey: .brandIdCounter)
        materialIdCounters = try container.decode([String: Int].self, forKey: .materialIdCounters)
        
        usedSerials = try container.decode([String].self, forKey: .usedSerials)
        spoolInstances = try container.decode([SpoolInstance].self, forKey: .spoolInstances)
        
        densityStandards = try container.decode([String: Double].self, forKey: .densityStandards)
        backupInfo = try container.decode(BackupInfo.self, forKey: .backupInfo)
        syncStatus = try container.decode(SyncStatus.self, forKey: .syncStatus)
    }
    
    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        
        try container.encode(version, forKey: .version)
        try container.encode(lastUpdated, forKey: .lastUpdated)
        try container.encode(databaseId, forKey: .databaseId)
        
        try container.encode(materials, forKey: .materials)
        try container.encode(brands, forKey: .brands)
        try container.encode(settings, forKey: .settings)
        
        try container.encode(brandIdCounter, forKey: .brandIdCounter)
        try container.encode(materialIdCounters, forKey: .materialIdCounters)
        
        try container.encode(usedSerials, forKey: .usedSerials)
        try container.encode(spoolInstances, forKey: .spoolInstances)
        
        try container.encode(densityStandards, forKey: .densityStandards)
        try container.encode(backupInfo, forKey: .backupInfo)
        try container.encode(syncStatus, forKey: .syncStatus)
    }
}

// MARK: - Supporting Structures

struct BackupInfo: Codable {
    var lastBackup: Date?
    var lastICloudSync: Date?
    var backupCount: Int = 0
}

struct SyncStatus: Codable {
    var lastCP6Sync: Date?
    var lastPrinterSync: Date?
    var machineProfile: String = "Creality K2 Pro 0.4 nozzle"
}
