#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "BLEManager.h"
#include "USBHIDManager.h"
#include "Config.h"

// Forward declaration to avoid circular include
class Bridge;

class WebManager {
public:
    /**
     * @brief Initialize WiFi AP and web server.
     */
    static void begin();

    /**
     * @brief Clean shutdown of WiFi AP and web server.
     * Stops server, disconnects clients, unmounts SPIFFS.
     * MUST be called before ESP.restart() for clean state.
     */
    static void end();

    /**
     * @brief Process pending web requests (if using sync server).
     */
    static void loop();

    /**
     * @brief Check if web manager is initialized.
     */
    static bool isInitialized() { return _initialized; }

    // Output manager setters - mutually exclusive
    static BLEManager *getBLEManager();
    static void setBLEManager(BLEManager *bleManager);
    static USBHIDManager *getUSBHIDManager();
    static void setUSBHIDManager(USBHIDManager *usbHidManager);

private:
    static AsyncWebServer _server;
    static BLEManager *_bleManager;
    static USBHIDManager *_usbHidManager;
    static bool _initialized;

    static void handleRoot(AsyncWebServerRequest *request);
    static void handleScript(AsyncWebServerRequest *request);
    static void handleKey(AsyncWebServerRequest *request);
    static void handleStatus(AsyncWebServerRequest *request);
    static void handleSlot(AsyncWebServerRequest *request);
    static void handleNotFound(AsyncWebServerRequest *request);
    static const char *getEmbeddedHTML();
    static const char *getEmbeddedJS();
};

#endif
