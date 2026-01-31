/**
 * @file USBHIDManager.h
 * @brief USB HID Keyboard output manager for MODE_USB.
 * Uses ESP32 Arduino's built-in USBHIDKeyboard library.
 * Only functional when compiled with ARDUINO_USB_MODE=1.
 */

#ifndef USB_HID_MANAGER_H
#define USB_HID_MANAGER_H

#include <Arduino.h>

// Only include USB libraries when in USB Device mode
#if ARDUINO_USB_MODE == 1
#include <USB.h>
#include <USBHIDKeyboard.h>
#endif

class USBHIDManager {
public:
  USBHIDManager();
  ~USBHIDManager();

  /**
   * @brief Initialize USB HID Device.
   * @param deviceName Device name for USB descriptor
   */
  void begin(const char *deviceName = "EKey");

  /**
   * @brief Clean shutdown of USB HID.
   */
  void end();

  /**
   * @brief Check if USB HID is initialized and ready.
   */
  bool isInitialized() const { return _initialized; }

  /**
   * @brief Check if USB is connected to a host.
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
  bool _initialized;

#if ARDUINO_USB_MODE == 1
  USBHIDKeyboard _keyboard;
#endif
};

#endif // USB_HID_MANAGER_H
