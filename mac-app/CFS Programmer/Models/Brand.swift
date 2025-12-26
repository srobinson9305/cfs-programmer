//
//  Brand.swift
//  CFS-Programmer
//
//  Created on 2025-12-24
//

import Foundation

struct Brand: Codable, Identifiable, Hashable {
    var id: String              // "0276" for Creality, "F001" for custom
    var name: String            // "Creality", "SUNLU", etc.
    var isCustom: Bool          // true if user-created
    var isOfficial: Bool        // true for Creality
    var createdDate: Date = Date()
    var notes: String = ""
    
    // Computed property for material count (calculated by database)
    var materialCount: Int {
        // This will be calculated by the database manager
        return 0
    }
    
    // MARK: - Hashable
    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
    
    static func == (lhs: Brand, rhs: Brand) -> Bool {
        return lhs.id == rhs.id
    }
}

// MARK: - Brand Extensions

extension Brand {
    /// Check if this is a custom brand (F### format)
    var isCustomBrand: Bool {
        return id.hasPrefix("F")
    }
    
    /// Get display name with official badge
    var displayName: String {
        return isOfficial ? "\(name) (Official)" : name
    }
}
