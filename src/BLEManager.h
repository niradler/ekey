#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include "NimBLEDevice.h"

// Service UUIDs
#define HID_SERVICE_UUID            "1812"
#define BATTERY_SERVICE_UUID        "180F"
#define DEVICE_INFO_SERVICE_UUID    "180A"

// HID Characteristic UUIDs
#define HID_INPUT_REPORT_UUID       "2A4D"
#define HID_REPORT_MAP_UUID         "2A4B"
#define HID_HID_INFORMATION_UUID    "2A4A"
#define HID_HID_CONTROL_POINT_UUID  "2A4C"
#define HID_PROTOCOL_MODE_UUID      "2A4E"
#define HID_REPORT_REFERENCE_UUID   "2908"

// Battery Characteristic UUID
#define BATTERY_LEVEL_UUID          "2A19"

// Device Info Characteristic UUIDs
#define MANUFACTURER_NAME_UUID      "2A29"
#define MODEL_NUMBER_UUID           "2A24"
#define PNP_ID_UUID                 "2A50"

class BLEManager {
public:
  BLEManager();
  ~BLEManager();

  /**
   * @brief Initialize BLE with given slot and device name.
   * Sets up HID, Battery, and Device Info services for cross-OS compatibility.
   */
  void begin(uint8_t slot, const char *deviceName);

  /**
   * @brief Clean shutdown of BLE stack.
   * Stops advertising, disconnects clients, deinitializes NimBLE.
   * MUST be called before ESP.restart() for clean state.
   */
  void end();

  /**
   * @brief Check if BLE is initialized and ready.
   */
  bool isInitialized() const { return _initialized; }

  /**
   * @brief Check if a BLE client is connected.
   */
  bool isConnected() const;

  /**
   * @brief Send a full keyboard report (6KRO).
   * @param keys Array of 6 HID keycodes
   * @param modifiers Modifier byte (Ctrl/Shift/Alt/GUI)
   */
  void sendKeyboardReport(const uint8_t *keys, uint8_t modifiers);

  /**
   * @brief Send a single key press and release.
   * @param keycode HID keycode
   * @param modifiers Modifier byte
   */
  void sendKey(uint8_t keycode, uint8_t modifiers = 0);

  /**
   * @brief Send key press (without release).
   */
  void sendKeyPress(uint8_t keycode, uint8_t modifiers = 0);

  /**
   * @brief Send key release (all keys up).
   */
  void sendKeyRelease();

private:
  // State
  bool _initialized;
  volatile bool _connected;

  // NimBLE objects (owned by NimBLE, we just hold pointers)
  NimBLEServer *_pServer;
  NimBLEService *_pHidService;
  NimBLEService *_pBatteryService;
  NimBLEService *_pDeviceInfoService;
  NimBLECharacteristic *_pInputReport;
  NimBLECharacteristic *_pBatteryLevel;

  // Callback (heap allocated, we own this)
  class ServerCallbacks;
  ServerCallbacks *_serverCallbacks;

  // Setup helpers
  void setUniqueMac(uint8_t slot);
  void setupHidService();
  void setupBatteryService();
  void setupDeviceInfoService();
  void setupAdvertising(const char *deviceName);
  void configureSecurityManager();

  // HID Report Map (standard keyboard)
  static const uint8_t _hidReportMap[];
  static const size_t _hidReportMapSize;

  // Server callback class
  class ServerCallbacks : public NimBLEServerCallbacks {
  public:
    ServerCallbacks(BLEManager *manager) : _manager(manager) {}
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason) override;
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override;
  private:
    BLEManager *_manager;
  };
};

#endif // BLE_MANAGER_H
