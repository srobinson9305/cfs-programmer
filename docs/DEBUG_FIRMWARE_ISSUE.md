# Debugging Guide - Firmware Version Not Showing

## Problem
Mac app shows "Firmware: Unknown" instead of the actual version.

## Root Cause
The firmware is not sending the VERSION message when the Mac app connects via BLE.

## Solution

### Step 1: Verify FIRMWARE_VERSION is defined

In your .ino file, you should have:

```cpp
#define FIRMWARE_VERSION "1.2.0"
```

### Step 2: Add Version Sending to onConnect Callback

Find or create your BLE server callbacks class:

```cpp
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("âœ… BLE Client Connected");

    // SEND VERSION IMMEDIATELY
    String versionMsg = "VERSION:" + String(FIRMWARE_VERSION);
    pTxCharacteristic->setValue(versionMsg.c_str());
    pTxCharacteristic->notify();

    delay(100); // Give Mac time to process

    Serial.print("ðŸ“¤ Sent version: ");
    Serial.println(FIRMWARE_VERSION);
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("âŒ BLE Client Disconnected");
    delay(500);
    pServer->startAdvertising();
  }
};
```

### Step 3: Set the Callbacks in setup()

```cpp
void setup() {
  // ... your setup code ...

  // BLE Setup
  BLEDevice::init("CFS-Programmer");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks()); // â­ THIS LINE

  // ... rest of setup ...
}
```

### Step 4: Test

1. Upload firmware
2. Open Serial Monitor (115200 baud)
3. Open Mac app
4. When it connects, you should see:
   ```
   âœ… BLE Client Connected
   ðŸ“¤ Sent version: 1.2.0
   ```
5. Mac app should show: "Firmware: 1.2.0"

## Alternative: Send on First Command

If you prefer, you can also send VERSION when the Mac app first requests it:

```cpp
// In your command handler
if (command == "GET_VERSION") {
  String versionMsg = "VERSION:" + String(FIRMWARE_VERSION);
  pTxCharacteristic->setValue(versionMsg.c_str());
  pTxCharacteristic->notify();
}
```

But the onConnect method is better because it's automatic!

## Debugging Tips

### Check Serial Monitor Output

When Mac app connects, you should see:
```
âœ… BLE Client Connected
ðŸ“¤ Sent version: 1.2.0
```

If you don't see this, the callback isn't being called.

### Check Mac App Console

In Xcode, when running the Mac app, check the console for:
```
[BLE] Message: VERSION:1.2.0
```

If you see this, the message was received but not parsed correctly.

### Check Mac App Code

In `BluetoothManager`, verify this exists:

```swift
func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
    guard characteristic.uuid == txUUID,
          let data = characteristic.value,
          let message = String(data: data, encoding: .utf8) else { return }

    // Handle VERSION message
    if message.hasPrefix("VERSION:") {
        let version = message.replacingOccurrences(of: "VERSION:", with: "")
        DispatchQueue.main.async {
            self.firmwareVersion = version  // â­ THIS LINE
        }
    }

    DispatchQueue.main.async {
        self.lastMessage = message
    }
}
```

## Common Issues

### Issue 1: Version Still Shows "Unknown"

**Cause:** Mac app not processing VERSION message

**Fix:** Check that the Mac app's `didUpdateValueFor` handler updates `firmwareVersion`

### Issue 2: Serial Shows Version Sent, but Mac Doesn't Receive

**Cause:** TX characteristic not set up correctly

**Fix:** Verify TX characteristic has Notify property:

```cpp
pTxCharacteristic = pService->createCharacteristic(
  CHARACTERISTIC_UUID_TX,
  BLECharacteristic::PROPERTY_NOTIFY
);
pTxCharacteristic->addDescriptor(new BLE2902());
```

### Issue 3: Connection Works But No Messages

**Cause:** Mac app not subscribed to notifications

**Fix:** In Mac app, ensure:

```swift
peripheral.setNotifyValue(true, for: characteristic)
```

## Quick Test

Run this in Serial Monitor after connecting:

```cpp
// Manually trigger version send
String versionMsg = "VERSION:1.2.0";
pTxCharacteristic->setValue(versionMsg.c_str());
pTxCharacteristic->notify();
```

If Mac app shows version now, the issue is with the onConnect callback.
