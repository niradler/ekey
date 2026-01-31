/**
 * @file Bridge.h
 * @brief The application logic that bridges USB HID keyboard inputs to BLE HID outputs.
 * Also handles device slot switching logic and mode management.
 */

#ifndef BRIDGE_H
#define BRIDGE_H

#include "BLEManager.h"
#include "Config.h"
#include "USBManager.h"
#include "USBHIDManager.h"
#include "ModeManager.h"
#include <Arduino.h>
#include <Preferences.h>

class Bridge {
public:
  /**
   * @brief Initializes the application based on current mode.
   * OTG Mode: Initializes USB Host + BLE
   * Web Mode: Initializes WiFi + WebServer + BLE
   * USB Mode: Initializes WiFi + WebServer + USB HID Device
   */
  static void begin();

  /**
   * @brief Clean shutdown of all components.
   * MUST be called before ESP.restart() for clean state.
   */
  static void end();

  /**
   * @brief Main loop for status reporting.
   */
  static void loop();

  /**
   * @brief Switches the active device slot.
   * Performs clean shutdown, saves state, and restarts ESP32.
   * @param slot New slot index (0-based).
   */
  static void switchToSlot(uint8_t slot);

  /**
   * @brief Switch between modes (OTG -> WEB -> USB -> OTG).
   * Performs clean shutdown, toggles mode, and restarts ESP32.
   */
  static void switchMode();

  /**
   * @brief Gets the current active slot index.
   * @return Current slot index (0-based).
   */
  static uint8_t getCurrentSlot();

  /**
   * @brief Gets a pointer to the BLE manager for external access.
   * @return Pointer to the BLEManager instance.
   */
  static BLEManager* getBLEManager();

  /**
   * @brief Gets a pointer to the USB HID manager for external access.
   * @return Pointer to the USBHIDManager instance.
   */
  static USBHIDManager* getUSBHIDManager();

private:
  static uint8_t _currentSlot;
  static BLEManager _bleManager;
  static USBHIDManager _usbHidManager;
  static Preferences _preferences;

  /** @brief Initialize OTG mode (USB Host + BLE). */
  static void initOTGMode();

  /** @brief Initialize Web mode (WiFi + WebServer + BLE). */
  static void initWebMode();

  /** @brief Initialize USB mode (WiFi + WebServer + USB HID Device). */
  static void initUSBMode();

  /** @brief Callback for processing USB keyboard reports. */
  static void onKeyboardReport(const uint8_t *data, size_t length);

  /** @brief Checks if the current keyboard input matches the device switch combo. */
  static bool checkDeviceSwitchCombo(const uint8_t *keys, uint8_t modifiers);
};

#endif // BRIDGE_H
