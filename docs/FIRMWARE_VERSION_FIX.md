
// ADD THIS TO YOUR FIRMWARE - onConnect Callback
// This ensures VERSION is sent immediately when Mac app connects

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("âœ… BLE Client Connected");

    // â­ CRITICAL: Send firmware version immediately on connect
    String versionMsg = "VERSION:" + String(FIRMWARE_VERSION);
    pTxCharacteristic->setValue(versionMsg.c_str());
    pTxCharacteristic->notify();

    delay(100); // Give Mac time to process the notification

    Serial.print("ðŸ“¤ Sent version: ");
    Serial.println(FIRMWARE_VERSION);
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("âŒ BLE Client Disconnected");

    // Restart advertising so another device can connect
    delay(500);
    pServer->startAdvertising();
    Serial.println("ðŸ”„ Restarted BLE advertising");
  }
};

// In your setup() function, make sure you set the callbacks:
void setup() {
  // ... your other setup code ...

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());  // â­ ADD THIS

  // ... rest of BLE setup ...
}
