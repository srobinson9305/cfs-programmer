//
//  ContentView.swift
//  CFS Programmer
//
//  Created by Steven Robinson on 12/20/25.
//

// CFS Programmer - Complete macOS App
// ContentView and supporting views
//
// This file should be used alongside the existing CFS_ProgrammerApp.swift

import SwiftUI
import CoreBluetooth
import Combine

// MARK: - Main Content View
struct ContentView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    @EnvironmentObject var materials: MaterialDatabase
    @State private var selectedTab = 0
    
    var body: some View {
        NavigationView {
            // Sidebar
            VStack(spacing: 0) {
                // Header
                HStack {
                    Image(systemName: "antenna.radiowaves.left.and.right")
                        .font(.title)
                        .foregroundColor(.blue)
                    Text("CFS Programmer")
                        .font(.title2)
                        .fontWeight(.bold)
                }
                .padding()
                
                // Connection Status
                ConnectionStatusView()
                
                Divider()
                
                // Navigation
                List(selection: $selectedTab) {
                    Label("Dashboard", systemImage: "gauge")
                        .tag(0)
                    Label("Read Tag", systemImage: "doc.text.magnifyingglass")
                        .tag(1)
                    Label("Write Tags", systemImage: "pencil.circle")
                        .tag(2)
                    Label("Wipe Tag", systemImage: "trash")
                        .tag(3)
                    Label("Materials", systemImage: "doc.text")
                        .tag(4)
                    Label("Settings", systemImage: "gearshape")
                        .tag(5)
                }
                .listStyle(.sidebar)
                
                Spacer()
                
                // Device Status
                if bluetooth.isConnected {
                    DeviceStatusView()
                }
            }
            .frame(width: 250)
            
            // Main Content
            Group {
                switch selectedTab {
                case 0:
                    DashboardView()
                case 1:
                    ReadTagView()
                case 2:
                    WriteTagView()
                case 3:
                    WipeTagView()
                case 4:
                    MaterialDatabaseView()
                case 5:
                    SettingsView()
                default:
                    DashboardView()
                }
            }
        }
    }
}

// MARK: - Connection Status View
struct ConnectionStatusView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    
    var body: some View {
        VStack(spacing: 12) {
            HStack {
                Circle()
                    .fill(bluetooth.isConnected ? Color.green : (bluetooth.isScanning ? Color.orange : Color.gray))
                    .frame(width: 12, height: 12)
                
                Text(statusText)
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
            
            if !bluetooth.isConnected && !bluetooth.isScanning {
                Button("Connect") {
                    bluetooth.startScanning()
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
            }
        }
        .padding()
        .background(Color(NSColor.controlBackgroundColor))
    }
    
    var statusText: String {
        if bluetooth.isConnected {
            return "Connected"
        } else if bluetooth.isScanning {
            return "Scanning..."
        } else {
            return "Disconnected"
        }
    }
}

// MARK: - Device Status View
struct DeviceStatusView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Divider()
            
            Text("Device Status")
                .font(.caption)
                .fontWeight(.semibold)
                .foregroundColor(.secondary)
                .padding(.horizontal)
            
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Circle()
                        .fill(Color.green)
                        .frame(width: 8, height: 8)
                    Text(bluetooth.deviceName)
                        .font(.caption)
                }
                
                HStack {
                    Image(systemName: "memorychip")
                        .font(.caption2)
                    Text(bluetooth.firmwareVersion)
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
            }
            .padding(.horizontal)
            .padding(.bottom)
        }
    }
}

// MARK: - Dashboard View
struct DashboardView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    @EnvironmentObject var materials: MaterialDatabase
    
    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                Text("CFS Programmer")
                    .font(.largeTitle)
                    .fontWeight(.bold)
                
                if bluetooth.isConnected {
                    Text("Device Connected")
                        .foregroundColor(.green)
                } else {
                    Text("Searching for device...")
                        .foregroundColor(.orange)
                }
                
                Divider()
                    .padding(.vertical)
                
                // Quick Actions
                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())], spacing: 20) {
                    QuickActionCard(
                        title: "Read Tag",
                        icon: "doc.text.magnifyingglass",
                        color: .blue,
                        description: "Scan and decode tag data"
                    )
                    
                    QuickActionCard(
                        title: "Write Tags",
                        icon: "pencil.circle",
                        color: .green,
                        description: "Program dual tag set"
                    )
                    
                    QuickActionCard(
                        title: "Wipe Tag",
                        icon: "trash",
                        color: .red,
                        description: "Reset to factory defaults"
                    )
                }
                .padding()
                
                Spacer()
            }
            .padding()
        }
    }
}

struct QuickActionCard: View {
    let title: String
    let icon: String
    let color: Color
    let description: String
    
    var body: some View {
        VStack(spacing: 12) {
            Image(systemName: icon)
                .font(.system(size: 40))
                .foregroundColor(color)
            
            Text(title)
                .font(.headline)
            
            Text(description)
                .font(.caption)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding()
        .background(Color(NSColor.controlBackgroundColor))
        .cornerRadius(12)
    }
}

// MARK: - Read Tag View
struct ReadTagView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    @State private var isReading = false
    @State private var tagInfo: TagInfo?
    
    struct TagInfo {
        let material: String
        let length: String
        let color: String
        let serial: String
        let isBlank: Bool
        let isCreality: Bool
    }
    
    var body: some View {
        VStack(spacing: 20) {
            Text("Read Tag")
                .font(.largeTitle)
                .fontWeight(.bold)
            
            Text("Scan and decode CFS tag information")
                .foregroundColor(.secondary)
            
            Divider()
            
            if let info = tagInfo {
                // Display tag info
                ScrollView {
                    VStack(spacing: 20) {
                        if !info.isCreality {
                            StatusBanner(
                                icon: "exclamationmark.triangle.fill",
                                title: "Not a Creality CFS Tag",
                                color: .orange
                            )
                        } else if info.isBlank {
                            StatusBanner(
                                icon: "doc",
                                title: "Blank Tag Detected",
                                color: .blue
                            )
                        } else {
                            // Valid tag display
                            GroupBox {
                                VStack(alignment: .leading, spacing: 16) {
                                    InfoRow(label: "Material", value: info.material)
                                    Divider()
                                    InfoRow(label: "Length", value: info.length)
                                    Divider()
                                    HStack {
                                        Text("Color:")
                                            .fontWeight(.semibold)
                                        Spacer()
                                        HStack {
                                            RoundedRectangle(cornerRadius: 4)
                                                .fill(Color(hex: info.color) ?? .gray)
                                                .frame(width: 30, height: 30)
                                            Text("#\(info.color)")
                                        }
                                    }
                                    Divider()
                                    InfoRow(label: "Serial", value: info.serial, mono: true)
                                }
                                .padding()
                            }
                        }
                        
                        Button("Read Another Tag") {
                            tagInfo = nil
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    .padding()
                }
            } else {
                // Read interface
                Spacer()
                
                VStack(spacing: 30) {
                    Image(systemName: isReading ? "wave.3.forward.circle.fill" : "wave.3.forward.circle")
                        .font(.system(size: 80))
                        .foregroundColor(isReading ? .blue : .gray)
                        .symbolEffect(.pulse, isActive: isReading)
                    
                    Text(isReading ? "Waiting for tag..." : "Ready to read")
                        .font(.title2)
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                Button(action: startReading) {
                    Label(isReading ? "Reading..." : "Read Tag", systemImage: "doc.text.magnifyingglass")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .disabled(isReading || !bluetooth.isConnected)
                .padding()
            }
        }
        .padding()
        .onChange(of: bluetooth.lastMessage) { _, newValue in
            handleMessage(newValue)
        }
    }
    
    func startReading() {
        isReading = true
        bluetooth.sendCommand("READ")
    }
    
    func handleMessage(_ message: String) {
        if message.hasPrefix("TAG_DATA:") {
            // Parse: PLA|165m|#FFFFFF|S/N:000001
            let data = message.replacingOccurrences(of: "TAG_DATA:", with: "")
            let parts = data.split(separator: "|")
            
            if parts.count >= 4 {
                tagInfo = TagInfo(
                    material: String(parts[0]),
                    length: String(parts[1]),
                    color: String(parts[2].dropFirst()),  // Remove #
                    serial: String(parts[3].replacingOccurrences(of: "S/N:", with: "")),
                    isBlank: false,
                    isCreality: true
                )
            }
            isReading = false
            
        } else if message.hasPrefix("ERROR:") {
            let error = message.replacingOccurrences(of: "ERROR:", with: "")
            
            if error.contains("Not a Creality") {
                tagInfo = TagInfo(material: "", length: "", color: "000000", serial: "", isBlank: false, isCreality: false)
            } else if error.contains("blank") {
                tagInfo = TagInfo(material: "", length: "", color: "000000", serial: "", isBlank: true, isCreality: true)
            }
            isReading = false
        }
    }
}

// MARK: - Write Tag View
struct WriteTagView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    @EnvironmentObject var materials: MaterialDatabase
    
    @State private var selectedMaterial: Material?
    @State private var selectedWeight: FilamentWeight = .grams500
    @State private var selectedColor = Color.white
    @State private var batchNumber = ""
    @State private var isWriting = false
    @State private var writeStep = 0  // 0=ready, 1=tag1, 2=tag2, 3=complete
    
    var body: some View {
        VStack(spacing: 20) {
            Text("Write Dual Tag Set")
                .font(.largeTitle)
                .fontWeight(.bold)
            
            Text("Configure and write matching front/back tags")
                .foregroundColor(.secondary)
            
            Divider()
            
            if writeStep == 0 {
                // Configuration
                Form {
                    Section("Material") {
                        Picker("Type", selection: $selectedMaterial) {
                            Text("Select material...").tag(nil as Material?)
                            ForEach(materials.materials) { material in
                                Text(material.name).tag(material as Material?)
                            }
                        }
                    }
                    
                    Section("Specifications") {
                        Picker("Weight", selection: $selectedWeight) {
                            ForEach(FilamentWeight.allCases) { weight in
                                Text(weight.displayName).tag(weight)
                            }
                        }
                        
                        ColorPicker("Color", selection: $selectedColor)
                        
                        TextField("Batch Number (2 chars)", text: $batchNumber)
                            .textFieldStyle(.roundedBorder)
                    }
                    
                    if selectedMaterial != nil {
                        Section("Preview") {
                            VStack(alignment: .leading, spacing: 8) {
                                InfoRow(label: "Material", value: selectedMaterial!.name)
                                InfoRow(label: "Weight", value: selectedWeight.displayName)
                                InfoRow(label: "Length", value: selectedWeight.lengthMeters)
                                HStack {
                                    Text("Color:")
                                    Spacer()
                                    Circle()
                                        .fill(selectedColor)
                                        .frame(width: 20, height: 20)
                                    Text(selectedColor.toHex())
                                }
                            }
                        }
                    }
                }
                .formStyle(.grouped)
                
                Spacer()
                
                Button(action: startWriting) {
                    Label("Write Dual Tag Set", systemImage: "square.on.square")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .disabled(selectedMaterial == nil || !bluetooth.isConnected)
                .padding()
                
            } else {
                // Writing progress
                VStack(spacing: 30) {
                    ProgressView(value: Double(writeStep), total: 3)
                        .progressViewStyle(.linear)
                    
                    Image(systemName: writeStep == 3 ? "checkmark.circle.fill" : "wave.3.forward.circle.fill")
                        .font(.system(size: 80))
                        .foregroundColor(writeStep == 3 ? .green : .blue)
                        .symbolEffect(.pulse, isActive: writeStep < 3)
                    
                    Text(writeStepText)
                        .font(.title2)
                    
                    if writeStep == 3 {
                        Button("Write Another Set") {
                            writeStep = 0
                        }
                        .buttonStyle(.borderedProminent)
                    }
                }
                
                Spacer()
            }
        }
        .padding()
        .onChange(of: bluetooth.lastMessage) { _, newValue in
            handleMessage(newValue)
        }
    }
    
    var writeStepText: String {
        switch writeStep {
        case 1: return "Place Tag 1 of 2..."
        case 2: return "Place Tag 2 of 2..."
        case 3: return "Complete! Both tags written ✓"
        default: return ""
        }
    }
    
    func startWriting() {
        guard let material = selectedMaterial else { return }
        
        let cfsData = generateCFSData(material: material, weight: selectedWeight, color: selectedColor, batch: batchNumber)
        
        writeStep = 1
        isWriting = true
        bluetooth.sendCommand("WRITE:\(cfsData)")
    }
    
    func handleMessage(_ message: String) {
        if message == "TAG1_OK" {
            writeStep = 2
        } else if message == "TAG2_OK" {
            writeStep = 3
            isWriting = false
        }
    }
    
    func generateCFSData(material: Material, weight: FilamentWeight, color: Color, batch: String) -> String {
        let dateCode = formatDateCode()
        let vendor = "0276"
        let batchCode = batch.padding(toLength: 2, withPad: "0", startingAt: 0)
        let filmID = material.filmamentID
        let colorHex = "0" + color.toHex()
        let length = weight.hexLength
        let serial = String(format: "%06d", Int.random(in: 1...999999))
        
        return "AB\(dateCode)\(vendor)\(batchCode)\(filmID)\(colorHex)\(length)\(serial)00000000000000"
    }
    
    func formatDateCode() -> String {
        let date = Date()
        let calendar = Calendar.current
        let month = calendar.component(.month, from: date)
        let day = calendar.component(.day, from: date)
        let year = calendar.component(.year, from: date) % 100
        
        return String(format: "%X%02d%02d", month, day, year)
    }
}

// MARK: - Wipe Tag View
struct WipeTagView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    @State private var isWiping = false
    @State private var showConfirmation = false
    @State private var wipeComplete = false
    
    var body: some View {
        VStack(spacing: 20) {
            Text("Wipe Tag")
                .font(.largeTitle)
                .fontWeight(.bold)
            
            Text("Reset tag to factory defaults")
                .foregroundColor(.secondary)
            
            Divider()
            
            StatusBanner(
                icon: "exclamationmark.triangle.fill",
                title: "Warning: This will permanently erase all data",
                color: .orange
            )
            
            Spacer()
            
            if wipeComplete {
                VStack(spacing: 30) {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.system(size: 80))
                        .foregroundColor(.green)
                    
                    Text("Tag Wiped Successfully")
                        .font(.title2)
                    
                    Button("Wipe Another Tag") {
                        wipeComplete = false
                    }
                    .buttonStyle(.borderedProminent)
                }
            } else if isWiping {
                VStack(spacing: 30) {
                    ProgressView()
                        .scaleEffect(2)
                    
                    Text("Place tag on reader...")
                        .font(.title2)
                }
            } else {
                VStack(spacing: 30) {
                    Image(systemName: "trash")
                        .font(.system(size: 80))
                        .foregroundColor(.gray)
                    
                    Text("Ready to wipe tag")
                        .font(.title2)
                        .foregroundColor(.secondary)
                }
            }
            
            Spacer()
            
            Button(action: { showConfirmation = true }) {
                Label("Wipe Tag", systemImage: "trash")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .tint(.red)
            .controlSize(.large)
            .disabled(isWiping || !bluetooth.isConnected)
            .padding()
            .confirmationDialog("Wipe Tag?", isPresented: $showConfirmation, titleVisibility: .visible) {
                Button("Wipe Tag", role: .destructive) {
                    startWiping()
                }
            } message: {
                Text("This will permanently erase all data. This cannot be undone.")
            }
        }
        .padding()
        .onChange(of: bluetooth.lastMessage) { _, newValue in
            if newValue == "WIPE_OK" {
                isWiping = false
                wipeComplete = true
            }
        }
    }
    
    func startWiping() {
        isWiping = true
        bluetooth.sendCommand("WIPE")
    }
}

// MARK: - Material Database View
struct MaterialDatabaseView: View {
    @EnvironmentObject var materials: MaterialDatabase
    @State private var showingAddMaterial = false
    
    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text("Material Database")
                    .font(.largeTitle)
                    .fontWeight(.bold)
                
                Spacer()
                
                Button(action: { showingAddMaterial = true }) {
                    Label("Add Material", systemImage: "plus")
                }
                .buttonStyle(.borderedProminent)
            }
            .padding()
            
            List {
                ForEach(materials.materials) { material in
                    HStack {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(material.name)
                                .font(.headline)
                            Text("ID: \(material.filmamentID)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        
                        Spacer()
                        
                        VStack(alignment: .trailing, spacing: 4) {
                            Text(material.vendor)
                            Text(material.category)
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding(.vertical, 4)
                }
            }
        }
        .sheet(isPresented: $showingAddMaterial) {
            AddMaterialView()
        }
    }
}

struct AddMaterialView: View {
    @Environment(\.dismiss) var dismiss
    @EnvironmentObject var materials: MaterialDatabase
    
    @State private var name = ""
    @State private var filmamentID = ""
    @State private var vendor = "Creality"
    @State private var category = "PLA"
    
    var body: some View {
        VStack(spacing: 20) {
            Text("Add New Material")
                .font(.title)
                .fontWeight(.bold)
            
            Form {
                TextField("Name", text: $name)
                TextField("Filament ID (6 digits)", text: $filmamentID)
                TextField("Vendor", text: $vendor)
                Picker("Category", selection: $category) {
                    Text("PLA").tag("PLA")
                    Text("PETG").tag("PETG")
                    Text("ABS").tag("ABS")
                    Text("TPU").tag("TPU")
                }
            }
            .formStyle(.grouped)
            
            HStack {
                Button("Cancel") { dismiss() }
                    .keyboardShortcut(.cancelAction)
                
                Spacer()
                
                Button("Add") {
                    materials.materials.append(Material(
                        name: name,
                        filmamentID: filmamentID,
                        vendor: vendor,
                        category: category
                    ))
                    materials.saveMaterials()
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(name.isEmpty || filmamentID.count != 6)
            }
            .padding()
        }
        .padding()
        .frame(width: 400, height: 400)
    }
}

// MARK: - Settings View
struct SettingsView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    
    var body: some View {
        Form {
            Section("Device") {
                InfoRow(label: "Name", value: bluetooth.deviceName)
                InfoRow(label: "Firmware", value: bluetooth.firmwareVersion)
                InfoRow(label: "Status", value: bluetooth.isConnected ? "Connected" : "Disconnected")
            }
            
            Section("About") {
                InfoRow(label: "App Version", value: "1.0.0")
                InfoRow(label: "Build", value: "2024.12")
            }
        }
        .formStyle(.grouped)
        .frame(maxWidth: 600)
        .padding()
    }
}

// MARK: - Helper Views
struct StatusBanner: View {
    let icon: String
    let title: String
    let color: Color
    
    var body: some View {
        HStack {
            Image(systemName: icon)
                .foregroundColor(color)
            Text(title)
                .font(.headline)
        }
        .padding()
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(color.opacity(0.1))
        .cornerRadius(8)
    }
}

struct InfoRow: View {
    let label: String
    let value: String
    var mono: Bool = false
    
    var body: some View {
        HStack {
            Text(label + ":")
                .fontWeight(.semibold)
            Spacer()
            Text(value)
                .font(mono ? .system(.body, design: .monospaced) : .body)
        }
    }
}

// MARK: - Data Models
struct Material: Identifiable, Codable, Hashable {
    var id = UUID()  // Changed to var for Codable
    var name: String
    var filmamentID: String
    var vendor: String
    var category: String
    
    // Hashable conformance
    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
    
    static func == (lhs: Material, rhs: Material) -> Bool {
        lhs.id == rhs.id
    }
}

enum FilamentWeight: String, CaseIterable, Identifiable {
    case grams250 = "250g"
    case grams500 = "500g"
    case grams600 = "600g"
    case grams750 = "750g"
    case kilograms1 = "1kg"
    
    var id: String { rawValue }
    var displayName: String { rawValue }
    
    var lengthMeters: String {
        switch self {
        case .grams250: return "82m"
        case .grams500: return "165m"
        case .grams600: return "198m"
        case .grams750: return "247m"
        case .kilograms1: return "330m"
        }
    }
    
    var hexLength: String {
        switch self {
        case .grams250: return "0082"
        case .grams500: return "0165"
        case .grams600: return "0198"
        case .grams750: return "0247"
        case .kilograms1: return "0330"
        }
    }
}

// MARK: - Material Database Manager
class MaterialDatabase: ObservableObject {
    @Published var materials: [Material] = [
        Material(name: "PLA", filmamentID: "101001", vendor: "Creality", category: "PLA"),
        Material(name: "PETG", filmamentID: "101002", vendor: "Creality", category: "PETG"),
        Material(name: "ABS", filmamentID: "101003", vendor: "Creality", category: "ABS"),
        Material(name: "TPU", filmamentID: "101004", vendor: "Creality", category: "TPU"),
        Material(name: "Nylon", filmamentID: "101005", vendor: "Creality", category: "Nylon")
    ]
    
    init() {
        loadMaterials()
    }
    
    func loadMaterials() {
        if let data = UserDefaults.standard.data(forKey: "materials"),
           let decoded = try? JSONDecoder().decode([Material].self, from: data) {
            materials = decoded
        }
    }
    
    func saveMaterials() {
        if let encoded = try? JSONEncoder().encode(materials) {
            UserDefaults.standard.set(encoded, forKey: "materials")
        }
    }
}

// MARK: - Bluetooth Manager with Auto-Connect
class BluetoothManager: NSObject, ObservableObject {
    @Published var isConnected = false
    @Published var isScanning = false
    @Published var deviceName = "CFS-Programmer"
    @Published var firmwareVersion = "Unknown"
    @Published var lastMessage = ""
    
    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var txCharacteristic: CBCharacteristic?
    private var rxCharacteristic: CBCharacteristic?
    
    private let serviceUUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private let rxUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
    private let txUUID = CBUUID(string: "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e")
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    func startScanning() {
        isScanning = true
        centralManager.scanForPeripherals(withServices: [serviceUUID])
    }
    
    func disconnect() {
        if let peripheral = peripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    func sendCommand(_ command: String) {
        guard let peripheral = peripheral,
              let rxChar = rxCharacteristic,
              let data = command.data(using: .utf8) else { return }
        
        peripheral.writeValue(data, for: rxChar, type: .withResponse)
    }
}

// MARK: - CBCentralManagerDelegate
extension BluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            startScanning()  // Auto-scan when ready
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        // Auto-connect to first CFS-Programmer found
        self.peripheral = peripheral
        centralManager.stopScan()
        isScanning = false
        centralManager.connect(peripheral)
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.delegate = self
        peripheral.discoverServices([serviceUUID])
        DispatchQueue.main.async {
            self.isConnected = true
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        DispatchQueue.main.async {
            self.isConnected = false
            self.startScanning()  // Auto-reconnect
        }
    }
}

// MARK: - CBPeripheralDelegate
extension BluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics([rxUUID, txUUID], for: service)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        
        for characteristic in characteristics {
            if characteristic.uuid == rxUUID {
                rxCharacteristic = characteristic
            } else if characteristic.uuid == txUUID {
                txCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value,
              let message = String(data: data, encoding: .utf8) else { return }
        
        DispatchQueue.main.async {
            self.lastMessage = message
            print("BLE RX: \(message)")
        }
    }
}

// MARK: - Color Extension
extension Color {
    func toHex() -> String {
        let components = NSColor(self).cgColor.components ?? [1, 1, 1]
        let r = Int(components[0] * 255)
        let g = Int(components[1] * 255)
        let b = Int(components[2] * 255)
        return String(format: "%02X%02X%02X", r, g, b)
    }
    
    init?(hex: String) {
        var hexSanitized = hex.trimmingCharacters(in: .whitespacesAndNewlines)
        hexSanitized = hexSanitized.replacingOccurrences(of: "#", with: "")
        
        var rgb: UInt64 = 0
        guard Scanner(string: hexSanitized).scanHexInt64(&rgb) else { return nil }
        
        let r = Double((rgb & 0xFF0000) >> 16) / 255.0
        let g = Double((rgb & 0x00FF00) >> 8) / 255.0
        let b = Double(rgb & 0x0000FF) / 255.0
        
        self.init(red: r, green: g, blue: b)
    }
}
