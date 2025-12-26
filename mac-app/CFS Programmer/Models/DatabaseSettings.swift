//
//  DatabaseSettings.swift
//  CFS Programmer
//
//  Created by Steven Robinson on 12/24/25.
//


//
//  DatabaseSettings.swift
//  CFS-Programmer
//
//  Created on 2025-12-24
//

import Foundation

struct DatabaseSettings: Codable {
    var enableICloudSync: Bool = false
    var enableDailySnapshots: Bool = true
    var machineProfile: String = "Creality K2 Pro 0.4 nozzle"
    var defaultDiameter: Double = 1.75
    var defaultCurrency: String = "USD"
    
    // SSH Settings for printer sync
    var printerSSH: SSHSettings = SSHSettings()
}

struct SSHSettings: Codable {
    var enabled: Bool = false
    var host: String = ""
    var port: Int = 22
    var username: String = "root"
    var authMethod: AuthMethod = .password
    var remotePath: String = "/mnt/UDISK/creality/userdata/box/material_database.json"
    var backupBeforeSync: Bool = true
}

enum AuthMethod: String, Codable {
    case password
    case key
}
