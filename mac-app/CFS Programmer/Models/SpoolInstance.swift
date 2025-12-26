//
//  SpoolInstance.swift
//  CFS-Programmer
//
//  Created on 2025-12-24
//

import Foundation

// Current SpoolInstance.swift (file:204)
struct SpoolInstance: Codable, Identifiable {
    var id: UUID = UUID()
    var serial: String
    var materialId: String
    var weightGrams: Int
    var writeDate: Date = Date()
    var location: String = ""
    var notes: String = ""
    var isActive: Bool = true
    var remainingWeight: Int?
    var dateWritten: Date = Date()
}

// MARK: - SpoolInstance Extensions

extension SpoolInstance {
    /// Format serial for display
    var formattedSerial: String {
        return "SN: \(serial)"
    }
    
    /// Get age in days
    var daysOld: Int {
        return Calendar.current.dateComponents([.day], from: dateWritten, to: Date()).day ?? 0
    }
}
