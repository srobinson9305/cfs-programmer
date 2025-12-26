//
//  AdvancedSettings.swift
//  CFS Programmer
//
//  Created by Steven Robinson on 12/24/25.
//


//
//  AdvancedSettings.swift
//  CFS-Programmer
//
//  Created on 2025-12-24
//

import Foundation

/// kvParam structure - organized by category for UI
struct AdvancedSettings: Codable {
    var temperatures: TemperatureParams = TemperatureParams()
    var speeds: SpeedParams = SpeedParams()
    var flow: FlowParams = FlowParams()
    var cooling: CoolingParams = CoolingParams()
    var retraction: RetractionParams = RetractionParams()
    var support: SupportParams = SupportParams()
    var quality: QualityParams = QualityParams()
    var other: [String: String] = [:]  // Any uncategorized params
}

// MARK: - Temperature Parameters

struct TemperatureParams: Codable {
    var bedTemperature: String = "60"
    var bedTemperatureInitialLayerSingle: String = "65"
    var chamberTemperature: String = "0"
    var nozzleTemperature: String = "210"
    var nozzleTemperatureInitialLayer: String = "215"
    var nozzleTemperatureRangeHigh: String = "230"
    var nozzleTemperatureRangeLow: String = "190"
    var texturedPlateTemp: String = "60"
    var texturedPlateTempInitialLayer: String = "65"
}

// MARK: - Speed Parameters

struct SpeedParams: Codable {
    var defaultAcceleration: String = "5000"
    var initialLayerAcceleration: String = "500"
    var outerWallAcceleration: String = "5000"
    var innerWallAcceleration: String = "5000"
    var bridgeSpeed: String = "25"
    var gapInfillingSpeed: String = "30"
    var infillSpeed: String = "200"
    var initialLayerSpeed: String = "50"
    var outerWallSpeed: String = "150"
    var innerWallSpeed: String = "200"
    var sparseInfillSpeed: String = "200"
    var supportSpeed: String = "150"
    var topSurfaceSpeed: String = "100"
    var travelSpeed: String = "300"
}

// MARK: - Flow Parameters

struct FlowParams: Codable {
    var filamentFlowRatio: String = "1"
    var outerWallLineWidth: String = "0.42"
    var innerWallLineWidth: String = "0.45"
    var sparseInfillLineWidth: String = "0.45"
    var internalSolidInfillLineWidth: String = "0.42"
    var topSurfaceLineWidth: String = "0.42"
    var supportLineWidth: String = "0.42"
    var initialLayerLineWidth: String = "0.42"
    var extrusionMultiplier: String = "1"
}

// MARK: - Cooling Parameters

struct CoolingParams: Codable {
    var fanCoolingLayerTime: String = "10"
    var fanMaxSpeed: String = "100"
    var fanMinSpeed: String = "20"
    var slowPrintingFanSpeedMin: String = "20"
    var fullFanSpeedLayer: String = "4"
    var closeHopperFanDelay: String = "0"
    var reduceInfillingRetraction: String = "1"
}

// MARK: - Retraction Parameters

struct RetractionParams: Codable {
    var retractionLength: String = "0.4"
    var retractionSpeed: String = "30"
    var zHop: String = "0"
    var zHopTypes: String = "Normal Lift"
    var retractWhenChangingLayer: String = "0"
    var wipingVolume: String = "0"
}

// MARK: - Support Parameters

struct SupportParams: Codable {
    var supportInterfaceSpacing: String = "0.2"
    var supportTopZDistance: String = "0.2"
    var supportBottomZDistance: String = "0.2"
    var supportAngle: String = "0"
    var supportLineWidth: String = "0.42"
    var supportInterfaceLineWidth: String = "0.42"
    var supportBasePatternSpacing: String = "2.5"
}

// MARK: - Quality Parameters

struct QualityParams: Codable {
    var detectThinWall: String = "0"
    var filterOutGapFill: String = "0"
    var gapFillSpeed: String = "30"
    var printSequence: String = "By Layer"
    var wallLoops: String = "2"
    var spiralMode: String = "0"
    var fuzzySkinfuzzyPattern: String = "None"
}

// MARK: - Advanced Settings Extensions

extension AdvancedSettings {
    /// Get all parameters as flat dictionary for CP6 export
    func flattenForCP6() -> [String: [String]] {
        var result: [String: [String]] = [:]
        
        // Helper to convert property to CP6 format
        func addParam(_ value: String, key: String) {
            result[key] = [value]
        }
        
        // Temperatures
        addParam(temperatures.bedTemperature, key: "bed_temperature")
        addParam(temperatures.bedTemperatureInitialLayerSingle, key: "bed_temperature_initial_layer_single")
        addParam(temperatures.chamberTemperature, key: "chamber_temperature")
        // ... add all other parameters
        
        return result
    }
    
    /// Import from flat CP6 dictionary
    mutating func importFromCP6(_ dict: [String: [String]]) {
        // Parse temperatures
        if let value = dict["bed_temperature"]?.first {
            temperatures.bedTemperature = value
        }
        // ... parse all other parameters
    }
}
