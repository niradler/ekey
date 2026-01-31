/**
 * @file Config.h
 * @brief Global configuration constants for EKey - Universal Keyboard Bridge.
 *
 * Modes:
 * 1. USB -> BLE: Physical USB keyboard to Bluetooth output
 * 2. Web -> BLE: Virtual web keyboard to Bluetooth output
 * 3. Web -> USB: Virtual web keyboard to USB HID output (TODO)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// CONFIGURATION - Edit these to customize behavior
// ============================================================================

/** @brief Device names for each slot (will show up in Bluetooth settings) */
#define DEVICE_NAME_1 "EKey-1"
#define DEVICE_NAME_2 "EKey-2"
#define DEVICE_NAME_3 "EKey-3"

/** @brief Manufacturer name reported over BLE */
#define DEVICE_MANUFACTURER "EKey"

/** @brief Battery level reported over BLE (0-100) */
#define BATTERY_LEVEL 100

/** @brief Number of available device slots (maximum 3 recommended) */
#define NUM_DEVICE_SLOTS 3

/** @brief Key combo for device switching: Scroll Lock + number key */
#define ENABLE_DEVICE_SWITCHING true

/**
 * @brief LED feedback pin (blinks to show current slot)
 * Set to -1 to disable, or use your board's built-in LED pin
 */
#define LED_FEEDBACK_PIN 2

/** @brief Operational modes */
#define MODE_OTG 0  // USB Keyboard -> BLE
#define MODE_WEB 1  // Web Virtual Keyboard -> BLE
#define MODE_USB 2  // Web Virtual Keyboard -> USB HID (TODO)
#define MODE_COUNT 3

/** @brief GPIO pin for mode toggle button (BOOT button) */
#define MODE_BUTTON_PIN 0

/** @brief WiFi AP SSID */
#define WIFI_AP_SSID "EKey"

/** @brief WiFi AP password (empty = open network) */
#define WIFI_AP_PASSWORD ""

/** @brief Web server port */
#define WEB_SERVER_PORT 80

#endif // CONFIG_H
