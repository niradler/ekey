/**
 * @file Config.h
 * @brief Global configuration constants for the USB-to-BLE Bridge.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// CONFIGURATION - Edit these to customize behavior
// ============================================================================

/** @brief Device names for each slot (will show up in Bluetooth settings) */
#define DEVICE_NAME_1 "esp-otg1"
#define DEVICE_NAME_2 "esp-otg2"
#define DEVICE_NAME_3 "esp-otg3"

/** @brief Manufacturer name reported over BLE */
#define DEVICE_MANUFACTURER "esp-otg"

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
#define MODE_OTG 0
#define MODE_WEB 1

/** @brief GPIO pin for mode toggle button (BOOT button) */
#define MODE_BUTTON_PIN 0

/** @brief WiFi AP SSID */
#define WIFI_AP_SSID "ESP32-BLE-Keyboard"

/** @brief WiFi AP password (empty = open network) */
#define WIFI_AP_PASSWORD ""

/** @brief Web server port */
#define WEB_SERVER_PORT 80

#endif // CONFIG_H
