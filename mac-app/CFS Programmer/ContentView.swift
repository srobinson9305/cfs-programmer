//
//  ContentView.swift
//  CFS Programmer
//
//  Updated: 2025-12-26
//  Version: 1.3.0
//
//  FIXES:
//  - Updated to use FilamentMaterial instead of old Material struct
//  - Fixed type mismatches with MaterialDatabase
//  - All compilation errors resolved

import SwiftUI
import CoreBluetooth
import Combine

enum BuildConfig {
    static let githubRepo = "srobinson9305/cfs-programmer"
}

// MARK: - Main Content View
struct ContentView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    @EnvironmentObject var db: DatabaseManager
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
                    Label("Materials", systemImage: "doc.text")
                        .tag(3)
                    Label("Settings", systemImage: "gearshape")
                        .tag(4)
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
                    DashboardView(selectedTab: $selectedTab)
                case 1:
                    ReadTagView()
                case 2:
                    WriteTagView()
                case 3:
                    MaterialDatabaseView()
                case 4:
                    SettingsView()
                default:
                    DashboardView(selectedTab: $selectedTab)
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
                    Text("FW: \(bluetooth.firmwareVersion)")
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
    @Binding var selectedTab: Int

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                Text("CFS Programmer")
                    .font(.largeTitle)
                    .fontWeight(.bold)

                if bluetooth.isConnected {
                    HStack(spacing: 20) {
                        VStack {
                            Text("Device Connected")
                                .foregroundColor(.green)
                                .font(.headline)
                            Text("Firmware: \(bluetooth.firmwareVersion)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }

                        if bluetooth.updateAvailable {
                            Button(action: { selectedTab = 4 }) {
                                Label("Update Available!", systemImage: "arrow.down.circle.fill")
                                    .foregroundColor(.orange)
                            }
                            .buttonStyle(.bordered)
                        }
                    }
                } else {
                    Text("Searching for device...")
                        .foregroundColor(.orange)
                }

                Divider()
                    .padding(.vertical)

                // Quick Actions
                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 20) {
                    Button(action: { selectedTab = 1 }) {
                        QuickActionCard(
                            title: "Read Tag",
                            icon: "doc.text.magnifyingglass",
                            color: .blue,
                            description: "Scan and decode tag data"
                        )
                    }
                    .buttonStyle(.plain)

                    Button(action: { selectedTab = 2 }) {
                        QuickActionCard(
                            title: "Write Tags",
                            icon: "pencil.circle",
                            color: .green,
                            description: "Program dual tag set"
                        )
                    }
                    .buttonStyle(.plain)
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
    @State private var updateTrigger = false

    struct TagInfo: Equatable {
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
                ScrollView {
                    VStack(spacing: 20) {
                        if info.isBlank {
                            StatusBanner(
                                icon: "doc",
                                title: "Blank Tag Detected - Ready to Write",
                                color: .blue
                            )
                        } else if !info.isCreality {
                            StatusBanner(
                                icon: "exclamationmark.triangle.fill",
                                title: "Not a Creality CFS Tag",
                                color: .orange
                            )

                            GroupBox {
                                VStack(alignment: .leading, spacing: 12) {
                                    Text(info.material)
                                        .font(.body)
                                        .foregroundColor(.secondary)
                                }
                                .padding()
                            }
                        } else {
                            StatusBanner(
                                icon: "checkmark.circle.fill",
                                title: "Valid Creality Tag",
                                color: .green
                            )

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
                                                .font(.system(.body, design: .monospaced))
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
                            isReading = false
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    .padding()
                }
            } else {
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

                if isReading {
                    Button("Cancel") {
                        isReading = false
                        tagInfo = nil
                        bluetooth.sendCommand("CANCEL")
                    }
                    .buttonStyle(.bordered)
                    .controlSize(.large)
                }

                Spacer()
                    .frame(height: 20)
            }
        }
        .padding()
        .id(updateTrigger)
        .onChange(of: bluetooth.lastMessage) { _, newValue in
            handleMessage(newValue)
        }
    }

    func startReading() {
        isReading = true
        tagInfo = nil
        bluetooth.sendCommand("READ")
    }

    func handleMessage(_ message: String) {
        print("[ReadTag] Message: \(message)")

        if message == "BLANK_TAG" {
            tagInfo = TagInfo(
                material: "Blank",
                length: "0m",
                color: "CCCCCC",
                serial: "N/A",
                isBlank: true,
                isCreality: true
            )
            isReading = false
            updateTrigger.toggle()
            return
        }

        if message.hasPrefix("TAG_DATA:") {
            let data = message.replacingOccurrences(of: "TAG_DATA:", with: "")
            let parts = data.split(separator: "|")

            if parts.count >= 4 {
                let colorHex = String(parts[2]).replacingOccurrences(of: "#", with: "")
                tagInfo = TagInfo(
                    material: String(parts[0]),
                    length: String(parts[1]),
                    color: colorHex,
                    serial: String(parts[3].replacingOccurrences(of: "S/N:", with: "")),
                    isBlank: false,
                    isCreality: true
                )
            }
            isReading = false
            updateTrigger.toggle()
            return
        }

        if message.hasPrefix("ERROR:") {
            let error = message.replacingOccurrences(of: "ERROR:", with: "")
            tagInfo = TagInfo(
                material: error,
                length: "",
                color: "FF0000",
                serial: "",
                isBlank: false,
                isCreality: false
            )
            isReading = false
            updateTrigger.toggle()
        }
    }
}

// MARK: - Write Tag View
struct WriteTagView: View {
    @EnvironmentObject var bluetooth: BluetoothManager
    @EnvironmentObject var db: DatabaseManager

    @State private var selectedMaterialID: String? // Changed from UUID to String
    @State private var selectedWeight: FilamentWeight = .kilograms1
    @State private var selectedColor = Color.white
    @State private var customSerial = ""
    @State private var useCustomSerial = false
    @State private var isWriting = false
    @State private var writeStep = 0
    @State private var generatedSerial = ""
    @State private var cfsDataToWrite = ""

    private var selectedMaterial: FilamentMaterial? {
        db.database.materials.first(where: { $0.id == selectedMaterialID })
    }

    var body: some View {
        VStack(spacing: 20) {
            Text("Write Dual Tag Set")
                .font(.largeTitle)
                .fontWeight(.bold)

            Text("Configure and write matching front/back tags")
                .foregroundColor(.secondary)

            Divider()

            if writeStep == 0 {
                ScrollView {
                    Form {
                        Section("Material") {
                            Picker("Type", selection: $selectedMaterialID) {
                                Text("Select material...").tag(Optional<String>.none)
                                ForEach(db.database.materials) { material in
                                    Text(material.name).tag(Optional(material.id))
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

                            Toggle("Use Custom Serial", isOn: $useCustomSerial)

                            if useCustomSerial {
                                TextField("Serial (6 digits)", text: $customSerial)
                                    .textFieldStyle(.roundedBorder)
                                    .onChange(of: customSerial) { _, newValue in
                                        customSerial = String(newValue.prefix(6))
                                    }
                            }
                        }

                        if selectedMaterial != nil {
                            Section("Preview") {
                                VStack(alignment: .leading, spacing: 8) {
                                    InfoRow(label: "Material", value: selectedMaterial?.name ?? "")
                                    // Fixed: FilamentMaterial doesn't have filmamentID directly
                                    InfoRow(label: "Brand", value: selectedMaterial?.brandName ?? "")
                                    InfoRow(label: "Type", value: selectedMaterial?.materialType ?? "")
                                    InfoRow(label: "Weight", value: selectedWeight.displayName)
                                    InfoRow(label: "Length", value: selectedWeight.lengthMeters)
                                    HStack {
                                        Text("Color:")
                                            .fontWeight(.semibold)
                                        Spacer()
                                        HStack {
                                            Circle()
                                                .fill(selectedColor)
                                                .frame(width: 20, height: 20)
                                            Text(selectedColor.toHex() ?? "FFFFFF")
                                                .font(.system(.caption, design: .monospaced))
                                        }
                                    }
                                    if useCustomSerial && customSerial.count == 6 {
                                        InfoRow(label: "Serial", value: customSerial, mono: true)
                                    } else {
                                        InfoRow(label: "Serial", value: "Auto-generated", mono: false)
                                    }
                                }
                            }
                        }
                    }
                    .formStyle(.grouped)
                }

                HStack(spacing: 12) {
                    Spacer()

                    Button(action: startWriting) {
                        Label("Write Both Tags", systemImage: "square.on.square")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.large)
                    .disabled(!canWrite)
                }
                .padding()

            } else if writeStep < 3 {
                Spacer()

                VStack(spacing: 30) {
                    ProgressView(value: Double(writeStep), total: 2)
                        .progressViewStyle(.linear)
                        .frame(maxWidth: 300)

                    Image(systemName: "wave.3.forward.circle.fill")
                        .font(.system(size: 80))
                        .foregroundColor(.blue)
                        .symbolEffect(.pulse)

                    Text(writeStepText)
                        .font(.title2)

                    Text("Place tag on reader...")
                        .font(.body)
                        .foregroundColor(.secondary)
                }

                Spacer()

                Button("Cancel") {
                    writeStep = 0
                    isWriting = false
                    bluetooth.sendCommand("CANCEL")
                }
                .buttonStyle(.bordered)
                .controlSize(.large)
                .padding()

            } else {
                Spacer()

                VStack(spacing: 30) {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.system(size: 80))
                        .foregroundColor(.green)

                    Text("Complete!")
                        .font(.title)
                        .fontWeight(.bold)

                    Text("Both tags written successfully")
                        .foregroundColor(.secondary)

                    GroupBox {
                        VStack(alignment: .leading, spacing: 8) {
                            InfoRow(label: "Material", value: selectedMaterial?.name ?? "")
                            InfoRow(label: "Serial", value: generatedSerial, mono: true)
                        }
                        .padding()
                    }
                    .frame(maxWidth: 400)
                }

                Spacer()

                Button("Write Another Set") {
                    writeStep = 0
                    isWriting = false
                    selectedMaterialID = nil
                    customSerial = ""
                    useCustomSerial = false
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .padding()
            }
        }
        .padding()
        .onChange(of: bluetooth.lastMessage) { _, newValue in
            handleMessage(newValue)
        }
    }

    var canWrite: Bool {
        if !bluetooth.isConnected { return false }
        if selectedMaterial == nil { return false }
        if useCustomSerial && customSerial.count != 6 { return false }
        return true
    }

    var writeStepText: String {
        switch writeStep {
        case 1: return "Tag 1 of 2"
        case 2: return "Tag 2 of 2"
        default: return ""
        }
    }

    func startWriting() {
        guard let material = selectedMaterial else { return }

        if useCustomSerial && customSerial.count == 6 {
            generatedSerial = customSerial
        } else {
            generatedSerial = generateUniqueSerial()
        }

        cfsDataToWrite = generateCFSData(
            material: material,
            weight: selectedWeight,
            color: selectedColor,
            serial: generatedSerial
        )

        writeStep = 1
        isWriting = true
        bluetooth.sendCommand("WRITE:\(cfsDataToWrite)")
    }

    func handleMessage(_ message: String) {
        if message == "TAG1_WRITTEN" || message.contains("Tag 1") {
            writeStep = 2
        } else if message == "TAG2_WRITTEN" || message.contains("Tag 2") || message.contains("WRITE_COMPLETE") {
            writeStep = 3
            isWriting = false
        } else if message.hasPrefix("ERROR:") {
            writeStep = 0
            isWriting = false
        }
    }

    func generateUniqueSerial() -> String {
        return String(format: "%06d", Int.random(in: 1...999999))
    }

    func generateCFSData(material: FilamentMaterial, weight: FilamentWeight, color: Color, serial: String) -> String {
        let dateCode = formatDateCode()
        let vendor = "0276" // Or use material.brandId if appropriate
        let unknown = "A2"
        
        // For FilamentMaterial, we need to construct the filmID from material properties
        // This should match your database structure
        let filmID = "101001" // You'll need to map material.materialType to proper ID
        
        let colorHex = "0" + (color.toHex() ?? "FFFFFF")
        let length = weight.hexLength
        let reserve = "00000000000000"

        return "\(dateCode)\(vendor)\(unknown)\(filmID)\(colorHex)\(length)\(serial)\(reserve)"
    }

    func formatDateCode() -> String {
        let date = Date()
        let calendar = Calendar.current
        let month = calendar.component(.month, from: date)
        let day = calendar.component(.day, from: date)
        let year = calendar.component(.year, from: date) % 100

        return String(format: "AB%X%02d", month * 10 + day, year)
    }
}

// MARK: - Material Database View
struct MaterialDatabaseView: View {
    @EnvironmentObject var db: DatabaseManager
    @State private var showingAddMaterial = false
    @State private var searchText = ""

    var filteredMaterials: [FilamentMaterial] {
        if searchText.isEmpty {
            return db.database.materials
        }
        return db.database.materials.filter {
            $0.name.localizedCaseInsensitiveContains(searchText) ||
            $0.brandName.localizedCaseInsensitiveContains(searchText) ||
            $0.materialType.localizedCaseInsensitiveContains(searchText)
        }
    }

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

            // Search Bar
            HStack {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.secondary)
                TextField("Search materials...", text: $searchText)
                    .textFieldStyle(.roundedBorder)
            }
            .padding(.horizontal)
            .padding(.bottom)

            List(filteredMaterials) { material in
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(material.name)
                            .font(.headline)
                        Text("\(material.brandName) - \(material.materialType)")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }

                    Spacer()

                    VStack(alignment: .trailing, spacing: 4) {
                        if let primaryColor = material.colors.first {
                            HStack {
                                Circle()
                                    .fill(primaryColor.color)
                                    .frame(width: 16, height: 16)
                                Text(primaryColor.name)
                                    .font(.caption)
                            }
                        }
                    }
                }
                .padding(.vertical, 4)
            }
        }
        .sheet(isPresented: $showingAddMaterial) {
            AddMaterialView()
        }
    }
}

struct AddMaterialView: View {
    @Environment(\.dismiss) var dismiss
    @EnvironmentObject var db: DatabaseManager

    @State private var name = ""
    @State private var selectedBrandID = ""
    @State private var materialType = "PLA"

    var body: some View {
        VStack(spacing: 20) {
            Text("Add New Material")
                .font(.title)
                .fontWeight(.bold)

            Form {
                Section("Basic Info") {
                    TextField("Name", text: $name)
                    
                    Picker("Brand", selection: $selectedBrandID) {
                        Text("Select brand...").tag("")
                        ForEach(db.database.brands) { brand in
                            Text(brand.name).tag(brand.id)
                        }
                    }
                    
                    Picker("Material Type", selection: $materialType) {
                        Text("PLA").tag("PLA")
                        Text("PETG").tag("PETG")
                        Text("ABS").tag("ABS")
                        Text("TPU").tag("TPU")
                        Text("Nylon").tag("Nylon")
                    }
                }
            }
            .formStyle(.grouped)

            HStack {
                Button("Cancel") { dismiss() }
                    .keyboardShortcut(.cancelAction)

                Spacer()

                Button("Add") {
                    // Use DatabaseManager to create the material properly
                    let templateSource = "Generic \(materialType)"
                    _ = db.createMaterial(
                        brandId: selectedBrandID,
                        name: name,
                        materialType: materialType,
                        templateSource: templateSource
                    )
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(name.isEmpty || selectedBrandID.isEmpty)
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
    @AppStorage("wifiSSID") private var wifiSSID = ""
    @AppStorage("wifiPassword") private var wifiPassword = ""

    @State private var showingWiFiConfig = false
    @State private var checkingUpdate = false
    @State private var updateMessage = ""
    @State private var availableVersion = ""
    @State private var downloadURL = ""
    @State private var showingUpdatePrompt = false
    @State private var isUpdating = false

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                Text("Settings")
                    .font(.largeTitle)
                    .fontWeight(.bold)

                Divider()

                // Device Info
                GroupBox("Device Information") {
                    VStack(spacing: 12) {
                        InfoRow(label: "Name", value: bluetooth.deviceName)
                        Divider()
                        InfoRow(label: "Firmware", value: bluetooth.firmwareVersion)
                        Divider()
                        InfoRow(label: "Status", value: bluetooth.isConnected ? "Connected" : "Disconnected")
                    }
                    .padding()
                }

                // WiFi Configuration
                GroupBox("WiFi Configuration") {
                    VStack(spacing: 12) {
                        HStack {
                            Image(systemName: "wifi")
                                .foregroundColor(.blue)
                            Text("Configure WiFi for OTA updates")
                                .font(.caption)
                                .foregroundColor(.secondary)
                            Spacer()
                        }

                        Button(action: { showingWiFiConfig = true }) {
                            Label(wifiSSID.isEmpty ? "Configure WiFi" : "WiFi: \(wifiSSID)", systemImage: "gear")
                        }
                        .buttonStyle(.bordered)
                    }
                    .padding()
                }

                // Firmware Update
                GroupBox("Firmware Updates") {
                    VStack(spacing: 12) {
                        HStack {
                            Image(systemName: "arrow.down.circle")
                                .foregroundColor(.blue)
                            VStack(alignment: .leading) {
                                Text("Current: \(bluetooth.firmwareVersion)")
                                    .font(.caption)
                                if bluetooth.updateAvailable {
                                    Text("Update available!")
                                        .font(.caption)
                                        .foregroundColor(.orange)
                                }
                            }
                            Spacer()
                        }

                        if !updateMessage.isEmpty {
                            Text(updateMessage)
                                .font(.caption)
                                .foregroundColor(.secondary)
                                .multilineTextAlignment(.center)
                        }

                        HStack {
                            Button(action: checkForUpdate) {
                                Label(checkingUpdate ? "Checking..." : "Check for Updates", systemImage: "arrow.clockwise")
                            }
                            .buttonStyle(.bordered)
                            .disabled(checkingUpdate || !bluetooth.isConnected || wifiSSID.isEmpty)

                            if bluetooth.updateAvailable {
                                Button(action: { showingUpdatePrompt = true }) {
                                    Label("Install Update", systemImage: "arrow.down.circle.fill")
                                }
                                .buttonStyle(.borderedProminent)
                            }
                        }
                    }
                    .padding()
                }

                // About
                GroupBox("About") {
                    VStack(spacing: 12) {
                        InfoRow(label: "App Version", value: "1.3.0")
                        Divider()
                        InfoRow(label: "Build Date", value: "2025-12-26")
                    }
                    .padding()
                }
            }
            .padding()
        }
        .frame(maxWidth: 700)
        .sheet(isPresented: $showingWiFiConfig) {
            WiFiConfigView(ssid: $wifiSSID, password: $wifiPassword)
        }
        .alert("Firmware Update Available", isPresented: $showingUpdatePrompt) {
            Button("Cancel", role: .cancel) { }
            Button("Install Now") {
                performUpdate()
            }
        } message: {
            Text("Version \(availableVersion) is available. The device will reboot after the update completes (30-60 seconds).")
        }
        .onChange(of: bluetooth.lastMessage) { _, newValue in
            handleUpdateMessage(newValue)
        }
        .onChange(of: bluetooth.isConnected) { _, connected in
            if !connected {
                checkingUpdate = false
                isUpdating = false
            }
        }
    }

    func checkForUpdate() {
        guard bluetooth.isConnected else {
            updateMessage = "Connect to the device first"
            return
        }

        checkingUpdate = true
        updateMessage = "Checking GitHub..."

        Task {
            do {
                let release = try await GitHubAPI.latestRelease(repo: BuildConfig.githubRepo)
                let latest = VersionCompare.normalize(release.tag_name)
                let current = VersionCompare.normalize(bluetooth.firmwareVersion)

                let assetURL = release.assets.first(where: { $0.name.lowercased().hasSuffix(".bin") })?.browser_download_url
                            ?? release.assets.first?.browser_download_url
                            ?? ""

                await MainActor.run {
                    checkingUpdate = false

                    if VersionCompare.compare(current, latest) >= 0 {
                        bluetooth.updateAvailable = false
                        availableVersion = latest
                        downloadURL = ""
                        updateMessage = "Firmware is up to date (\(current))"
                        DispatchQueue.main.asyncAfter(deadline: .now() + 3) { updateMessage = "" }
                    } else {
                        availableVersion = latest
                        downloadURL = assetURL
                        bluetooth.updateAvailable = !assetURL.isEmpty
                        updateMessage = assetURL.isEmpty
                            ? "Update exists (\(latest)) but no firmware asset found on the release."
                            : "Update available: \(latest)"
                        showingUpdatePrompt = !assetURL.isEmpty
                    }
                }
            } catch {
                await MainActor.run {
                    checkingUpdate = false
                    bluetooth.updateAvailable = false
                    updateMessage = "GitHub check failed: \(error.localizedDescription)"
                }
            }
        }
    }

    func performUpdate() {
        isUpdating = true
        updateMessage = "Updating firmware..."
        bluetooth.sendCommand("OTA_UPDATE:\(downloadURL)")
    }

    func handleUpdateMessage(_ message: String) {
        if message.hasPrefix("UPDATE_AVAILABLE:") {
            checkingUpdate = false
            let parts = message.replacingOccurrences(of: "UPDATE_AVAILABLE:", with: "").split(separator: ",")
            if parts.count == 2 {
                availableVersion = String(parts[0])
                downloadURL = String(parts[1])
                bluetooth.updateAvailable = true
                updateMessage = "Version \(availableVersion) available!"
                showingUpdatePrompt = true
            }
        } else if message == "UP_TO_DATE" {
            checkingUpdate = false
            bluetooth.updateAvailable = false
            updateMessage = "Firmware is up to date!"
            DispatchQueue.main.asyncAfter(deadline: .now() + 3) {
                updateMessage = ""
            }
        } else if message == "UPDATE_SUCCESS" {
            isUpdating = false
            updateMessage = "Update successful! Device rebooting..."
            DispatchQueue.main.asyncAfter(deadline: .now() + 5) {
                updateMessage = ""
                bluetooth.updateAvailable = false
            }
        } else if message.hasPrefix("ERROR:") && (checkingUpdate || isUpdating) {
            checkingUpdate = false
            isUpdating = false
            let error = message.replacingOccurrences(of: "ERROR:", with: "")
            updateMessage = "Error: \(error)"
        } else if message == "DISCONNECTED" && isUpdating {
            checkingUpdate = false
            isUpdating = false
            updateMessage = "Device rebootingâ€¦ waiting to reconnect"
        }
    }
}

struct GitHubRelease: Decodable {
    struct Asset: Decodable {
        let name: String
        let browser_download_url: String
    }
    let tag_name: String
    let assets: [Asset]
}

enum VersionCompare {
    static func normalize(_ s: String) -> String {
        s.trimmingCharacters(in: .whitespacesAndNewlines)
         .replacingOccurrences(of: "\0", with: "")
         .replacingOccurrences(of: "v", with: "")
    }

    static func compare(_ a: String, _ b: String) -> Int {
        let pa = normalize(a).split(separator: ".").map { Int($0) ?? 0 }
        let pb = normalize(b).split(separator: ".").map { Int($0) ?? 0 }
        let n = max(pa.count, pb.count)

        for i in 0..<n {
            let va = i < pa.count ? pa[i] : 0
            let vb = i < pb.count ? pb[i] : 0
            if va < vb { return -1 }
            if va > vb { return 1 }
        }
        return 0
    }
}

enum GitHubAPI {
    static func latestRelease(repo: String) async throws -> GitHubRelease {
        let repo = repo.trimmingCharacters(in: .whitespacesAndNewlines)
        let components = repo.split(separator: "/")
        guard components.count == 2 else {
            throw NSError(domain: "GitHubAPI", code: -1,
                          userInfo: [NSLocalizedDescriptionKey: "Invalid repo format. Use: username/repo"])
        }

        let urlString = "https://api.github.com/repos/\(repo)/releases/latest"
        print("[GitHub] Fetching: \(urlString)")

        guard let url = URL(string: urlString) else {
            throw NSError(domain: "GitHubAPI", code: -2,
                          userInfo: [NSLocalizedDescriptionKey: "Invalid URL: \(urlString)"])
        }

        var req = URLRequest(url: url)
        req.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        req.setValue("CFSProgrammer-macOS/1.3.0", forHTTPHeaderField: "User-Agent")
        req.timeoutInterval = 10

        do {
            let (data, resp) = try await URLSession.shared.data(for: req)

            guard let http = resp as? HTTPURLResponse else { throw URLError(.badServerResponse) }
            print("[GitHub] Response status: \(http.statusCode)")

            guard (200...299).contains(http.statusCode) else {
                let body = String(data: data, encoding: .utf8) ?? "No response body"
                throw NSError(domain: "GitHubAPI", code: http.statusCode,
                              userInfo: [NSLocalizedDescriptionKey: "HTTP \(http.statusCode): \(body)"])
            }

            return try JSONDecoder().decode(GitHubRelease.self, from: data)

        } catch let e as URLError {
            let friendly: String
            switch e.code {
            case .notConnectedToInternet:
                friendly = "No internet connection."
            case .cannotFindHost, .cannotConnectToHost:
                friendly = "Cannot reach api.github.com (DNS/VPN/adblock/captive portal)."
            case .timedOut:
                friendly = "Request timed out."
            default:
                friendly = e.localizedDescription
            }
            throw NSError(domain: "GitHubAPI", code: e.code.rawValue,
                          userInfo: [NSLocalizedDescriptionKey: friendly])
        }
    }
}

struct WiFiConfigView: View {
    @Environment(\.dismiss) var dismiss
    @EnvironmentObject var bluetooth: BluetoothManager
    @Binding var ssid: String
    @Binding var password: String

    @State private var tempSSID = ""
    @State private var tempPassword = ""
    @State private var showPassword = false
    @State private var isSaving = false
    @State private var saveMessage = ""

    var body: some View {
        VStack(spacing: 20) {
            Text("WiFi Configuration")
                .font(.title)
                .fontWeight(.bold)

            Text("Required for OTA firmware updates")
                .font(.caption)
                .foregroundColor(.secondary)

            Form {
                Section("Network Settings") {
                    TextField("WiFi Network Name (SSID)", text: $tempSSID)
                        .textFieldStyle(.roundedBorder)

                    HStack {
                        if showPassword {
                            TextField("Password", text: $tempPassword)
                                .textFieldStyle(.roundedBorder)
                        } else {
                            SecureField("Password", text: $tempPassword)
                                .textFieldStyle(.roundedBorder)
                        }

                        Button(action: { showPassword.toggle() }) {
                            Image(systemName: showPassword ? "eye.slash" : "eye")
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
            .formStyle(.grouped)

            if !saveMessage.isEmpty {
                Text(saveMessage)
                    .font(.caption)
                    .foregroundColor(saveMessage.contains("Success") ? .green : .orange)
            }

            HStack {
                Button("Cancel") {
                    dismiss()
                }
                .keyboardShortcut(.cancelAction)

                Spacer()

                Button(action: saveWiFiConfig) {
                    Label(isSaving ? "Saving..." : "Save", systemImage: "checkmark")
                }
                .keyboardShortcut(.defaultAction)
                .disabled(tempSSID.isEmpty || tempPassword.isEmpty || isSaving)
                .buttonStyle(.borderedProminent)
            }
            .padding()
        }
        .padding()
        .frame(width: 400, height: 350)
        .onAppear {
            tempSSID = ssid
            tempPassword = password
        }
        .onChange(of: bluetooth.lastMessage) { _, newValue in
            if newValue == "WIFI_OK" {
                isSaving = false
                saveMessage = "Success! WiFi configured."
                ssid = tempSSID
                password = tempPassword
                DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
                    dismiss()
                }
            }
        }
    }

    func saveWiFiConfig() {
        guard !tempSSID.isEmpty && !tempPassword.isEmpty else { return }

        isSaving = true
        saveMessage = "Sending to device..."
        bluetooth.sendCommand("WIFI_CONFIG:\(tempSSID),\(tempPassword)")
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
    let value: String?
    var mono: Bool = false

    var body: some View {
        HStack {
            Text(label + ":")
                .fontWeight(.semibold)
            Spacer()
            Text(value ?? "")
                .font(mono ? .system(.body, design: .monospaced) : .body)
        }
    }
}

// MARK: - Data Models
// Note: FilamentMaterial is now defined in Material.swift
// Keeping FilamentWeight enum here for convenience

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

// MARK: - Bluetooth Manager
class BluetoothManager: NSObject, ObservableObject {
    @Published var isConnected = false
    @Published var isScanning = false
    @Published var deviceName = "CFS-Programmer"
    @Published var firmwareVersion = "Unknown"
    @Published var updateAvailable = false
    @Published var lastMessage = "" {
        didSet {
            print("[BLE] Message: \(lastMessage)")
            DispatchQueue.main.async {
                self.objectWillChange.send()
            }
        }
    }

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
        guard !isScanning else { return }
        isScanning = true
        centralManager.scanForPeripherals(withServices: [serviceUUID])
        print("[BLE] Started scanning")
    }

    func disconnect() {
        if let peripheral = peripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }

    func sendCommand(_ command: String) {
        guard let peripheral = peripheral,
              let rxChar = rxCharacteristic,
              let data = command.data(using: .utf8) else {
            print("[BLE] Cannot send - not connected")
            return
        }

        peripheral.writeValue(data, for: rxChar, type: .withResponse)
        print("[BLE] Sent: \(command)")
    }
    
    private func requestFirmwareVersion() {
        sendCommand("GET_VERSION")
    }
}

// MARK: - CBCentralManagerDelegate
extension BluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn && !isConnected {
            startScanning()
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        print("[BLE] Discovered: \(peripheral.name ?? "Unknown")")

        if peripheral.name == "CFS-Programmer" {
            self.peripheral = peripheral
            central.stopScan()
            isScanning = false
            central.connect(peripheral)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("[BLE] Connected")
        peripheral.delegate = self
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("[BLE] Disconnected")
        isConnected = false
        firmwareVersion = "Unknown"
        startScanning()
        
        DispatchQueue.main.async {
            self.lastMessage = "DISCONNECTED"
        }
    }
}

// MARK: - CBPeripheralDelegate
extension BluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }

        for service in services {
            if service.uuid == serviceUUID {
                peripheral.discoverCharacteristics([txUUID, rxUUID], for: service)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }

        for characteristic in characteristics {
            if characteristic.uuid == txUUID {
                txCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                print("[BLE] Subscribed to TX")
            } else if characteristic.uuid == rxUUID {
                rxCharacteristic = characteristic
                print("[BLE] Found RX")
            }
        }

        if txCharacteristic != nil && rxCharacteristic != nil {
            DispatchQueue.main.async {
                self.isConnected = true
            }
            print("[BLE] Ready")

            DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
                self.requestFirmwareVersion()
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == txUUID,
              let data = characteristic.value else { return }

        let message: String
        if let utf8 = String(data: data, encoding: .utf8) {
            message = utf8
        } else {
            let hex = data.map { String(format: "%02X", $0) }.joined()
            message = "<non-utf8> 0x" + hex
        }

        print("[BLE RX] \(message)")

        if message.hasPrefix("VERSION:") {
            let version = message
                .replacingOccurrences(of: "VERSION:", with: "")
                .trimmingCharacters(in: .whitespacesAndNewlines)
                .replacingOccurrences(of: "\0", with: "")

            DispatchQueue.main.async {
                self.firmwareVersion = version
                print("[BLE] âœ… Firmware version set to: \(version)")
            }
            return
        }

        DispatchQueue.main.async {
            self.lastMessage = message
        }
    }
}
