//
//  LengthCalculator.swift
//  CFS Programmer
//
//  Created by Steven Robinson on 12/24/25.
//


//
//  LengthCalculator.swift
//  CFS-Programmer
//
//  Created on 2025-12-24
//

import Foundation

/// Utility for calculating filament length and weight conversions
struct LengthCalculator {
    
    // MARK: - Length Calculation
    
    /// Calculate filament length in meters from weight and density
    /// - Parameters:
    ///   - weightGrams: Weight of filament in grams
    ///   - density: Material density in g/cm³
    ///   - diameter: Filament diameter in mm (default 1.75)
    /// - Returns: Length in meters (rounded to nearest meter)
    static func calculateLength(
        weightGrams: Double,
        density: Double,
        diameter: Double = 1.75
    ) -> Int {
        // Formula: Length = (Weight / Density) / (π * (diameter/2)²)
        // This gives us: Volume / Cross-sectional area = Length
        
        let radiusMm = diameter / 2.0
        let crossSectionMm2 = Double.pi * radiusMm * radiusMm
        let crossSectionCm2 = crossSectionMm2 / 100.0
        
        // Calculate volume in cm³
        let volumeCm3 = weightGrams / density
        
        // Calculate length in cm, then convert to meters
        let lengthCm = volumeCm3 / crossSectionCm2
        let lengthM = lengthCm / 100.0
        
        return Int(round(lengthM))
    }
    
    /// Calculate weight in grams from length and density
    /// - Parameters:
    ///   - lengthMeters: Length of filament in meters
    ///   - density: Material density in g/cm³
    ///   - diameter: Filament diameter in mm (default 1.75)
    /// - Returns: Weight in grams
    static func calculateWeight(
        lengthMeters: Double,
        density: Double,
        diameter: Double = 1.75
    ) -> Int {
        let radiusMm = diameter / 2.0
        let crossSectionMm2 = Double.pi * radiusMm * radiusMm
        let crossSectionCm2 = crossSectionMm2 / 100.0
        
        // Convert length to cm
        let lengthCm = lengthMeters * 100.0
        
        // Calculate volume in cm³
        let volumeCm3 = lengthCm * crossSectionCm2
        
        // Calculate weight
        let weightGrams = volumeCm3 * density
        
        return Int(round(weightGrams))
    }
    
    // MARK: - Hex Conversion
    
    /// Convert length in meters to 4-digit hex string for RFID tag
    /// - Parameter lengthMeters: Length in meters
    /// - Returns: 4-digit hex string (e.g., "014A" for 330m)
    static func lengthToHex(_ lengthMeters: Int) -> String {
        return String(format: "%04X", lengthMeters)
    }
    
    /// Convert hex string to length in meters
    /// - Parameter hex: 4-digit hex string
    /// - Returns: Length in meters, or nil if invalid
    static func hexToLength(_ hex: String) -> Int? {
        return Int(hex, radix: 16)
    }
    
    // MARK: - Standard Weights
    
    /// Get standard weight options for a material
    /// - Parameters:
    ///   - density: Material density in g/cm³
    ///   - diameter: Filament diameter in mm
    /// - Returns: Array of WeightOption structs
    static func standardWeightOptions(
        density: Double,
        diameter: Double = 1.75
    ) -> [WeightOption] {
        let weights = [1000, 750, 500, 250]
        
        return weights.map { grams in
            let lengthMeters = calculateLength(
                weightGrams: Double(grams),
                density: density,
                diameter: diameter
            )
            let lengthHex = lengthToHex(lengthMeters)
            
            return WeightOption(
                grams: grams,
                lengthMeters: lengthMeters,
                lengthHex: lengthHex,
                isDefault: grams == 1000
            )
        }
    }
    
    // MARK: - Cost Calculations
    
    /// Calculate cost per meter from cost per kg
    /// - Parameters:
    ///   - costPerKg: Cost per kilogram
    ///   - density: Material density in g/cm³
    ///   - diameter: Filament diameter in mm
    /// - Returns: Cost per meter
    static func calculateCostPerMeter(
        costPerKg: Double,
        density: Double,
        diameter: Double = 1.75
    ) -> Double {
        // Weight of 1 meter in grams
        let weightPerMeter = calculateWeight(
            lengthMeters: 1.0,
            density: density,
            diameter: diameter
        )
        
        // Cost per gram
        let costPerGram = costPerKg / 1000.0
        
        // Cost per meter
        return Double(weightPerMeter) * costPerGram
    }
    
    /// Calculate total cost for a spool
    /// - Parameters:
    ///   - weightGrams: Spool weight in grams
    ///   - costPerKg: Cost per kilogram
    /// - Returns: Total cost
    static func calculateSpoolCost(weightGrams: Int, costPerKg: Double) -> Double {
        return (Double(weightGrams) / 1000.0) * costPerKg
    }
    
    // MARK: - Validation
    
    /// Validate density value
    /// - Parameter density: Density to validate
    /// - Returns: True if density is reasonable for filament
    static func isValidDensity(_ density: Double) -> Bool {
        return density >= 0.5 && density <= 2.5
    }
    
    /// Validate diameter value
    /// - Parameter diameter: Diameter to validate
    /// - Returns: True if diameter is a standard size
    static func isValidDiameter(_ diameter: Double) -> Bool {
        let standardDiameters = [1.75, 2.85, 3.0]
        return standardDiameters.contains(where: { abs($0 - diameter) < 0.1 })
    }
    
    // MARK: - Formatting
    
    /// Format length for display
    /// - Parameter meters: Length in meters
    /// - Returns: Formatted string (e.g., "330m")
    static func formatLength(_ meters: Int) -> String {
        return "\(meters)m"
    }
    
    /// Format weight for display
    /// - Parameter grams: Weight in grams
    /// - Returns: Formatted string (e.g., "1kg", "750g")
    static func formatWeight(_ grams: Int) -> String {
        if grams >= 1000 && grams % 1000 == 0 {
            return "\(grams / 1000)kg"
        } else if grams >= 1000 {
            let kg = Double(grams) / 1000.0
            return String(format: "%.1fkg", kg)
        } else {
            return "\(grams)g"
        }
    }
    
    /// Format cost for display
    /// - Parameters:
    ///   - cost: Cost value
    ///   - currency: Currency code (default USD)
    /// - Returns: Formatted string (e.g., "$19.99")
    static func formatCost(_ cost: Double, currency: String = "USD") -> String {
        let formatter = NumberFormatter()
        formatter.numberStyle = .currency
        formatter.currencyCode = currency
        formatter.minimumFractionDigits = 2
        formatter.maximumFractionDigits = 2
        
        return formatter.string(from: NSNumber(value: cost)) ?? "$0.00"
    }
}

// MARK: - Extensions

extension WeightOption {
    /// Get formatted display string
    var displayString: String {
        return "\(LengthCalculator.formatWeight(grams)) (\(LengthCalculator.formatLength(lengthMeters)))"
    }
    
    /// Get short display string
    var shortDisplayString: String {
        return LengthCalculator.formatWeight(grams)
    }
}
