#include "BLEManager.h"
#include "Config.h"
#include <esp_mac.h>

// Standard HID Keyboard Report Map
// Includes: 8 modifier bits, 1 reserved byte, 6 keycodes, 5 LED outputs
const uint8_t BLEManager::_hidReportMap[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)

    // Modifier keys (8 bits)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0xE0,        //   Usage Minimum (224) - Left Ctrl
    0x29, 0xE7,        //   Usage Maximum (231) - Right GUI
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)

    // Reserved byte
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)

    // LED output (5 bits + 3 padding)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (1) - Num Lock
    0x29, 0x05,        //   Usage Maximum (5) - Kana
    0x91, 0x02,        //   Output (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x01,        //   Output (Constant) - Padding

    // Key array (6 keys)
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data, Array)

    0xC0               // End Collection
};
const size_t BLEManager::_hidReportMapSize = sizeof(_hidReportMap);

BLEManager::BLEManager()
    : _initialized(false)
    , _connected(false)
    , _pServer(nullptr)
    , _pHidService(nullptr)
    , _pBatteryService(nullptr)
    , _pDeviceInfoService(nullptr)
    , _pInputReport(nullptr)
    , _pBatteryLevel(nullptr)
    , _serverCallbacks(nullptr)
{
}

BLEManager::~BLEManager() {
    if (_initialized) {
        end();
    }
}

void BLEManager::begin(uint8_t slot, const char *deviceName) {
    if (_initialized) {
        Serial.println("[BLE] Already initialized, call end() first");
        return;
    }

    Serial.printf("[BLE] Initializing slot %d: '%s'\n", slot + 1, deviceName);

    // 1. Set unique MAC address per slot
    setUniqueMac(slot);

    // 2. Initialize NimBLE
    NimBLEDevice::init(deviceName);

    // 3. Configure security for bonding (critical for iOS/Android)
    configureSecurityManager();

    // 4. Create server
    _pServer = NimBLEDevice::createServer();
    if (!_pServer) {
        Serial.println("[BLE] ERROR: Failed to create server!");
        return;
    }

    // 5. Set up callbacks
    _serverCallbacks = new ServerCallbacks(this);
    _pServer->setCallbacks(_serverCallbacks);

    // 6. Create all required services (order matters for some clients)
    setupDeviceInfoService();  // Windows needs this
    setupBatteryService();     // Windows needs this
    setupHidService();         // Core HID functionality

    // 7. Start services
    _pDeviceInfoService->start();
    _pBatteryService->start();
    _pHidService->start();

    // 8. Start server
    _pServer->start();

    // 9. Configure and start advertising
    setupAdvertising(deviceName);

    _initialized = true;
    Serial.println("[BLE] Initialization complete - device should be discoverable");
}

void BLEManager::end() {
    if (!_initialized) {
        Serial.println("[BLE] Not initialized, skipping cleanup");
        return;
    }

    Serial.println("[BLE] Beginning cleanup...");

    // 1. Stop advertising first
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    if (pAdvertising) {
        pAdvertising->stop();
        Serial.println("[BLE] Advertising stopped");
    }

    // 2. Update connection state
    _connected = false;

    // 3. Allow time for disconnect notifications
    delay(100);

    // 4. Full NimBLE deinit
    // Parameter true = clear all bonding info from RAM (NVS already saved separately)
    NimBLEDevice::deinit(true);
    Serial.println("[BLE] NimBLE deinitialized");

    // 5. Clean up our callback object
    if (_serverCallbacks) {
        delete _serverCallbacks;
        _serverCallbacks = nullptr;
    }

    // 6. Reset all pointers (NimBLE freed the objects)
    _pServer = nullptr;
    _pHidService = nullptr;
    _pBatteryService = nullptr;
    _pDeviceInfoService = nullptr;
    _pInputReport = nullptr;
    _pBatteryLevel = nullptr;

    _initialized = false;
    Serial.println("[BLE] Cleanup complete");
}

bool BLEManager::isConnected() const {
    return _connected && _pServer && _pServer->getConnectedCount() > 0;
}

void BLEManager::configureSecurityManager() {
    // Security configuration for broad OS compatibility
    // Reference: https://esp32.com/viewtopic.php?t=31207

    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);  // "Just Works" pairing
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

    Serial.println("[BLE] Security configured: Bonding + Secure Connections");
}

void BLEManager::setUniqueMac(uint8_t slot) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Modify last byte based on slot (preserves vendor prefix)
    mac[5] = (mac[5] & 0xF0) | (slot & 0x0F);

    esp_err_t err = esp_base_mac_addr_set(mac);
    if (err != ESP_OK) {
        Serial.printf("[BLE] Warning: Failed to set MAC: %s\n", esp_err_to_name(err));
    } else {
        Serial.printf("[BLE] MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

void BLEManager::setupHidService() {
    _pHidService = _pServer->createService(HID_SERVICE_UUID);

    // Protocol Mode (Report Protocol = 1)
    NimBLECharacteristic *pProtocolMode = _pHidService->createCharacteristic(
        HID_PROTOCOL_MODE_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR
    );
    uint8_t protocolMode = 1;  // Report Protocol
    pProtocolMode->setValue(&protocolMode, 1);

    // Report Map (defines HID report structure)
    // Encrypted read for iOS compatibility
    NimBLECharacteristic *pReportMap = _pHidService->createCharacteristic(
        HID_REPORT_MAP_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC
    );
    pReportMap->setValue(_hidReportMap, _hidReportMapSize);

    // HID Information
    NimBLECharacteristic *pHidInfo = _pHidService->createCharacteristic(
        HID_HID_INFORMATION_UUID,
        NIMBLE_PROPERTY::READ
    );
    // bcdHID = 0x0111 (HID 1.11), bCountryCode = 0, Flags = 0x02 (normally connectable)
    uint8_t hidInfo[] = {0x11, 0x01, 0x00, 0x02};
    pHidInfo->setValue(hidInfo, sizeof(hidInfo));

    // HID Control Point
    NimBLECharacteristic *pControlPoint = _pHidService->createCharacteristic(
        HID_HID_CONTROL_POINT_UUID,
        NIMBLE_PROPERTY::WRITE_NR
    );
    uint8_t controlPoint = 0;
    pControlPoint->setValue(&controlPoint, 1);

    // Input Report (keyboard data)
    // Encrypted read for iOS compatibility
    _pInputReport = _pHidService->createCharacteristic(
        HID_INPUT_REPORT_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC
    );

    // Report Reference Descriptor (Report ID = 1, Input = 1)
    NimBLEDescriptor *pReportRef = _pInputReport->createDescriptor(
        HID_REPORT_REFERENCE_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC,
        2
    );
    uint8_t reportRef[] = {0x01, 0x01};  // Report ID 1, Input Report
    pReportRef->setValue(reportRef, 2);

    Serial.println("[BLE] HID Service configured");
}

void BLEManager::setupBatteryService() {
    _pBatteryService = _pServer->createService(BATTERY_SERVICE_UUID);

    _pBatteryLevel = _pBatteryService->createCharacteristic(
        BATTERY_LEVEL_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    uint8_t batteryLevel = BATTERY_LEVEL;  // From Config.h
    _pBatteryLevel->setValue(&batteryLevel, 1);

    Serial.println("[BLE] Battery Service configured");
}

void BLEManager::setupDeviceInfoService() {
    _pDeviceInfoService = _pServer->createService(DEVICE_INFO_SERVICE_UUID);

    // Manufacturer Name
    NimBLECharacteristic *pManufacturer = _pDeviceInfoService->createCharacteristic(
        MANUFACTURER_NAME_UUID,
        NIMBLE_PROPERTY::READ
    );
    pManufacturer->setValue(DEVICE_MANUFACTURER);

    // Model Number
    NimBLECharacteristic *pModel = _pDeviceInfoService->createCharacteristic(
        MODEL_NUMBER_UUID,
        NIMBLE_PROPERTY::READ
    );
    pModel->setValue("ESP32-S3 USB-BLE Bridge");

    // PnP ID (Vendor ID Source, Vendor ID, Product ID, Product Version)
    // Using Apple's VID/PID for maximum compatibility
    NimBLECharacteristic *pPnpId = _pDeviceInfoService->createCharacteristic(
        PNP_ID_UUID,
        NIMBLE_PROPERTY::READ
    );
    // Vendor ID Source: 0x02 (USB), Vendor ID: 0x05AC (Apple), Product ID: 0x820A, Version: 0x0001
    uint8_t pnpId[] = {0x02, 0xAC, 0x05, 0x0A, 0x82, 0x01, 0x00};
    pPnpId->setValue(pnpId, sizeof(pnpId));

    Serial.println("[BLE] Device Info Service configured");
}

void BLEManager::setupAdvertising(const char *deviceName) {
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();

    // Clear any previous configuration
    pAdvertising->reset();

    // Add HID service UUID (primary service)
    pAdvertising->addServiceUUID(HID_SERVICE_UUID);

    // Set device name
    pAdvertising->setName(deviceName);

    // Set appearance - CRITICAL for Windows 11!
    pAdvertising->setAppearance(0x03C1);  // Keyboard

    // Enable scan response for full device name
    pAdvertising->enableScanResponse(true);

    // Connection parameters for HID (fast response)
    // Min interval: 7.5ms, Max interval: 15ms (in 1.25ms units)
    pAdvertising->setPreferredParams(0x06, 0x0C);

    // Start advertising
    bool started = pAdvertising->start();
    if (started) {
        Serial.printf("[BLE] Advertising started as '%s'\n", deviceName);
    } else {
        Serial.println("[BLE] ERROR: Failed to start advertising!");
        // Retry once
        delay(100);
        started = pAdvertising->start();
        Serial.printf("[BLE] Advertising retry: %s\n", started ? "SUCCESS" : "FAILED");
    }
}

void BLEManager::sendKeyboardReport(const uint8_t *keys, uint8_t modifiers) {
    if (!isConnected() || !_pInputReport) {
        return;
    }

    uint8_t report[8] = {0};
    report[0] = modifiers;
    // report[1] = 0 (reserved)
    if (keys) {
        memcpy(&report[2], keys, 6);
    }

    _pInputReport->setValue(report, sizeof(report));
    if (!_pInputReport->notify()) {
        Serial.println("[BLE] Warning: notify() failed");
    }
}

void BLEManager::sendKey(uint8_t keycode, uint8_t modifiers) {
    sendKeyPress(keycode, modifiers);
    delay(10);  // Hold time
    sendKeyRelease();
}

void BLEManager::sendKeyPress(uint8_t keycode, uint8_t modifiers) {
    if (!isConnected() || !_pInputReport) {
        return;
    }

    uint8_t report[8] = {0};
    report[0] = modifiers;
    if (keycode > 0) {
        report[2] = keycode;
    }

    _pInputReport->setValue(report, sizeof(report));
    _pInputReport->notify();
}

void BLEManager::sendKeyRelease() {
    if (!isConnected() || !_pInputReport) {
        return;
    }

    uint8_t report[8] = {0};
    _pInputReport->setValue(report, sizeof(report));
    _pInputReport->notify();
}

// Server Callbacks Implementation
void BLEManager::ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo) {
    _manager->_connected = true;
    // Update connection parameters for HID stability
    // Min: 7.5ms, Max: 15ms, Latency: 0, Timeout: 4s
    pServer->updateConnParams(connInfo.getConnHandle(), 0x06, 0x0C, 0, 400);
}

void BLEManager::ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason) {
    _manager->_connected = false;
    // NimBLE auto-restarts advertising
}

void BLEManager::ServerCallbacks::onAuthenticationComplete(NimBLEConnInfo& connInfo) {
    if (connInfo.isBonded()) {
        Serial.println("[BLE] Bonded");
    }
}
