#include "Bridge.h"
#include "NVSUtils.h"
#include "ModeManager.h"
#include "WebManager.h"

// Only include USB Host HID header when in USB Host mode
#if ARDUINO_USB_MODE == 0
#include <hid_usage_keyboard.h>
#endif

uint8_t Bridge::_currentSlot = 0;
BLEManager Bridge::_bleManager;
USBHIDManager Bridge::_usbHidManager;
Preferences Bridge::_preferences;

void Bridge::begin() {
  // 1. Load saved slot
  if (!_preferences.begin("usb-ble", false)) {
    Serial.println("[Bridge] Warning: Failed to open preferences");
  }
  _currentSlot = _preferences.getUChar("slot", 0);
  if (_currentSlot >= NUM_DEVICE_SLOTS)
    _currentSlot = 0;
  _preferences.end();

  Serial.printf("[Config] Starting on device slot %d\n", _currentSlot + 1);
  Serial.printf("[Config] Mode: %s\n", ModeManager::getModeName());

  // 2. Initialize based on mode
  switch (ModeManager::getMode()) {
    case MODE_OTG:
      initOTGMode();
      break;
    case MODE_WEB:
      initWebMode();
      break;
    case MODE_USB:
      initUSBMode();
      break;
    default:
      Serial.println("[Bridge] ERROR: Unknown mode, defaulting to OTG");
      ModeManager::setMode(MODE_OTG);
      initOTGMode();
      break;
  }
}

void Bridge::end() {
  Serial.println("[Bridge] Beginning cleanup sequence...");

  // Mode-specific cleanup
  switch (ModeManager::getMode()) {
    case MODE_OTG:
      if (USBManager::isInitialized()) {
        USBManager::end();
      }
      // BLE cleanup
      NVSUtils::saveSlotBonds(_currentSlot);
      if (_bleManager.isInitialized()) {
        _bleManager.end();
      }
      break;

    case MODE_WEB:
      if (WebManager::isInitialized()) {
        WebManager::end();
      }
      // BLE cleanup
      NVSUtils::saveSlotBonds(_currentSlot);
      if (_bleManager.isInitialized()) {
        _bleManager.end();
      }
      break;

    case MODE_USB:
      if (WebManager::isInitialized()) {
        WebManager::end();
      }
      if (_usbHidManager.isInitialized()) {
        _usbHidManager.end();
      }
      // No BLE bonds to save in USB mode
      break;
  }

  Serial.println("[Bridge] Cleanup sequence complete");
}

void Bridge::initOTGMode() {
  Serial.println("[Mode] OTG Mode - Initializing USB Host + BLE");

  // 1. Load bonds for this slot
  NVSUtils::loadSlotBonds(_currentSlot);

  // 2. Init BLE
  const char *deviceNames[NUM_DEVICE_SLOTS] = {DEVICE_NAME_1, DEVICE_NAME_2,
                                               DEVICE_NAME_3};
  _bleManager.begin(_currentSlot, deviceNames[_currentSlot]);

  // 3. Init USB Host
  USBManager::setKeyboardCallback(onKeyboardReport);
  USBManager::begin();

  Serial.println("[Mode] OTG Mode initialization complete");
}

void Bridge::initWebMode() {
  Serial.println("[Mode] Web Mode - Initializing WiFi AP + WebServer + BLE");

  // 1. Load bonds for this slot
  NVSUtils::loadSlotBonds(_currentSlot);

  // 2. Init BLE
  const char *deviceNames[NUM_DEVICE_SLOTS] = {DEVICE_NAME_1, DEVICE_NAME_2,
                                               DEVICE_NAME_3};
  _bleManager.begin(_currentSlot, deviceNames[_currentSlot]);

  // 3. Init Web Manager with BLE reference
  WebManager::setBLEManager(&_bleManager);
  WebManager::begin();

  Serial.println("[Mode] Web Mode initialization complete");
}

void Bridge::initUSBMode() {
  Serial.println("[Mode] USB Mode - Initializing WiFi AP + WebServer + USB HID");

#if ARDUINO_USB_MODE != 1
  Serial.println("[Mode] ERROR: USB Device mode not enabled in this build!");
  Serial.println("[Mode] This firmware was compiled with ARDUINO_USB_MODE=0 (Host mode)");
  Serial.println("[Mode] USB mode requires firmware compiled with ARDUINO_USB_MODE=1");
  Serial.println("[Mode] Falling back to WEB mode with BLE output");
  ModeManager::setMode(MODE_WEB);
  initWebMode();
  return;
#else
  // 1. Init USB HID Device
  const char *deviceNames[NUM_DEVICE_SLOTS] = {DEVICE_NAME_1, DEVICE_NAME_2,
                                               DEVICE_NAME_3};
  _usbHidManager.begin(deviceNames[_currentSlot]);

  // 2. Init Web Manager with USB HID reference
  WebManager::setUSBHIDManager(&_usbHidManager);
  WebManager::begin();

  Serial.println("[Mode] USB Mode initialization complete");
#endif
}

void Bridge::loop() {
  // Handle Web mode or USB mode (both use WebManager for input)
  if (ModeManager::isWebMode() || ModeManager::isUSBMode()) {
    WebManager::loop();
  }

  // Track and report connection state changes
  static bool lastConnected = false;
  bool connected = false;

  if (ModeManager::isUSBMode()) {
    connected = _usbHidManager.isConnected();
  } else {
    connected = _bleManager.isConnected();
  }

  if (connected != lastConnected) {
    lastConnected = connected;
    const char* outputType = ModeManager::isUSBMode() ? "USB" : "BLE";
    Serial.printf("[%s] %s\n", outputType, connected ? "CONNECTED" : "Disconnected");
  }
}

uint8_t Bridge::getCurrentSlot() { return _currentSlot; }

BLEManager *Bridge::getBLEManager() { return &_bleManager; }

USBHIDManager *Bridge::getUSBHIDManager() { return &_usbHidManager; }

void Bridge::switchToSlot(uint8_t slot) {
  if (slot >= NUM_DEVICE_SLOTS)
    return;

  if (slot == _currentSlot) {
    Serial.printf("[Slot] Already on slot %d\n", slot + 1);
    if (LED_FEEDBACK_PIN >= 0) {
      for (int i = 0; i <= slot; i++) {
        digitalWrite(LED_FEEDBACK_PIN, HIGH);
        delay(150);
        digitalWrite(LED_FEEDBACK_PIN, LOW);
        delay(150);
      }
    }
    return;
  }

  Serial.printf("[Slot] Switching from slot %d to slot %d\n", _currentSlot + 1,
                slot + 1);

  // 1. Save new slot index before cleanup
  if (_preferences.begin("usb-ble", false)) {
    _preferences.putUChar("slot", slot);
    _preferences.end();
  }

  // 2. LED feedback
  if (LED_FEEDBACK_PIN >= 0) {
    for (int i = 0; i <= slot; i++) {
      digitalWrite(LED_FEEDBACK_PIN, HIGH);
      delay(150);
      digitalWrite(LED_FEEDBACK_PIN, LOW);
      delay(150);
    }
  }

  // 3. Clean shutdown of all components
  end();

  Serial.println("[System] Restarting to apply new slot settings...");
  delay(500);
  ESP.restart();
}

void Bridge::switchMode() {
  Serial.println("[Mode] Switching mode...");

  // 1. Toggle mode in NVS
  ModeManager::toggleMode();
  Serial.printf("[Mode] New mode will be: %s\n", ModeManager::getModeName());

  // 2. Warn if mode requires different firmware
#if ARDUINO_USB_MODE == 0
  if (ModeManager::isUSBMode()) {
    Serial.println("[Mode] WARNING: USB mode requires firmware with ARDUINO_USB_MODE=1");
    Serial.println("[Mode] Current firmware only supports OTG and WEB modes");
    Serial.println("[Mode] USB mode will fall back to WEB mode on next boot");
  }
#else
  if (ModeManager::isOTGMode()) {
    Serial.println("[Mode] WARNING: OTG mode requires firmware with ARDUINO_USB_MODE=0");
    Serial.println("[Mode] Current firmware only supports USB mode");
  }
#endif

  // 3. Clean shutdown of all components
  end();

  Serial.println("[System] Restarting to apply new mode...");
  delay(500);
  ESP.restart();
}

// Only compile USB Host keyboard handling when in USB Host mode
#if ARDUINO_USB_MODE == 0

void Bridge::onKeyboardReport(const uint8_t *data, size_t length) {
  if (length < sizeof(hid_keyboard_input_report_boot_t))
    return;

  hid_keyboard_input_report_boot_t *kb_report =
      (hid_keyboard_input_report_boot_t *)data;

  // Check for device switching combo
  if (checkDeviceSwitchCombo(kb_report->key, kb_report->modifier.val)) {
    return;
  }

  // Debug output
  Serial.printf("[KB] mod:0x%02X keys:[%02X %02X %02X %02X %02X %02X]\n",
                kb_report->modifier.val, kb_report->key[0], kb_report->key[1],
                kb_report->key[2], kb_report->key[3], kb_report->key[4],
                kb_report->key[5]);

  // Forward to BLE
  _bleManager.sendKeyboardReport(kb_report->key, kb_report->modifier.val);
}

bool Bridge::checkDeviceSwitchCombo(const uint8_t *keys, uint8_t modifiers) {
  if (!ENABLE_DEVICE_SWITCHING)
    return false;

  bool hasRightAlt = (modifiers & 0x40) != 0;
  uint8_t numberKey = 0;

  for (int i = 0; i < 6; i++) {
    if (keys[i] >= HID_KEY_1 && keys[i] <= HID_KEY_3) {
      numberKey = keys[i] - HID_KEY_1 + 1;
      break;
    }
  }

  if (hasRightAlt && numberKey > 0 && numberKey <= NUM_DEVICE_SLOTS) {
    Serial.printf("[Switch] Right Alt + %d detected\n", numberKey);
    switchToSlot(numberKey - 1);
    return true;
  }

  return false;
}

#else
// Stub implementations when USB Host mode not available

void Bridge::onKeyboardReport(const uint8_t *data, size_t length) {
  // Not used in USB Device mode
}

bool Bridge::checkDeviceSwitchCombo(const uint8_t *keys, uint8_t modifiers) {
  return false;
}

#endif
