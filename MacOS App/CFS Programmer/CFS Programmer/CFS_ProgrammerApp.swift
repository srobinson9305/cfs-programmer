//
//  CFS_ProgrammerApp.swift
//  CFS Programmer
//
//  Created by Steven Robinson on 12/20/25.
//

// CFS_ProgrammerApp.swift
// Main app entry point

import SwiftUI

@main
struct CFS_ProgrammerApp: App {
    @StateObject private var bluetoothManager = BluetoothManager()
    @StateObject private var materialDatabase = MaterialDatabase()
    
    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(bluetoothManager)
                .environmentObject(materialDatabase)
                .frame(minWidth: 1000, minHeight: 700)
        }
        .commands {
            CommandGroup(replacing: .newItem) {}
            
            CommandMenu("Device") {
                Button("Scan for Device") {
                    bluetoothManager.startScanning()
                }
                .keyboardShortcut("r", modifiers: [.command, .shift])
                
                Button("Disconnect") {
                    bluetoothManager.disconnect()
                }
                .keyboardShortcut("d", modifiers: .command)
                .disabled(!bluetoothManager.isConnected)
            }
        }
    }
}
