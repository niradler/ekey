#include "USBHIDManager.h"
#include "Config.h"

USBHIDManager::USBHIDManager()
    : _initialized(false)
{
}

USBHIDManager::~USBHIDManager() {
  if (_initialized) {
    end();
  }
}

#if ARDUINO_USB_MODE == 1

void USBHIDManager::begin(const char *deviceName) {
  if (_initialized) {
    Serial.println("[USB-HID] Already initialized");
    return;
  }

  Serial.printf("[USB-HID] Initializing as '%s'\n", deviceName);

  // Initialize USB with custom device name
  USB.productName(deviceName);
  USB.manufacturerName(DEVICE_MANUFACTURER);
  USB.begin();

  // Initialize HID Keyboard
  _keyboard.begin();

  _initialized = true;
  Serial.println("[USB-HID] Initialization complete - ready for host connection");
}

void USBHIDManager::end() {
  if (!_initialized) {
    Serial.println("[USB-HID] Not initialized, skipping cleanup");
    return;
  }

  Serial.println("[USB-HID] Beginning cleanup...");

  _keyboard.end();

  _initialized = false;
  Serial.println("[USB-HID] Cleanup complete");
}

bool USBHIDManager::isConnected() const {
  return _initialized && USB;
}

void USBHIDManager::sendKeyboardReport(const uint8_t *keys, uint8_t modifiers) {
  if (!isConnected()) return;

  // Build HID report: [modifier, reserved, key1-key6]
  KeyReport report;
  report.modifiers = modifiers;
  report.reserved = 0;
  if (keys) {
    memcpy(report.keys, keys, 6);
  } else {
    memset(report.keys, 0, 6);
  }

  _keyboard.sendReport(&report);
}

void USBHIDManager::sendKey(uint8_t keycode, uint8_t modifiers) {
  sendKeyPress(keycode, modifiers);
  delay(10);  // Hold time - match BLEManager behavior
  sendKeyRelease();
}

void USBHIDManager::sendKeyPress(uint8_t keycode, uint8_t modifiers) {
  if (!isConnected()) return;

  KeyReport report;
  report.modifiers = modifiers;
  report.reserved = 0;
  memset(report.keys, 0, 6);
  if (keycode > 0) {
    report.keys[0] = keycode;
  }

  _keyboard.sendReport(&report);
}

void USBHIDManager::sendKeyRelease() {
  if (!isConnected()) return;

  KeyReport report;
  memset(&report, 0, sizeof(report));
  _keyboard.sendReport(&report);
}

#else
// Stub implementations when USB Device mode not available

void USBHIDManager::begin(const char *deviceName) {
  Serial.println("[USB-HID] ERROR: USB Device mode not enabled in build!");
  Serial.println("[USB-HID] Use firmware compiled with ARDUINO_USB_MODE=1 for MODE_USB");
}

void USBHIDManager::end() {}
bool USBHIDManager::isConnected() const { return false; }
void USBHIDManager::sendKeyboardReport(const uint8_t *keys, uint8_t modifiers) {}
void USBHIDManager::sendKey(uint8_t keycode, uint8_t modifiers) {}
void USBHIDManager::sendKeyPress(uint8_t keycode, uint8_t modifiers) {}
void USBHIDManager::sendKeyRelease() {}

#endif
