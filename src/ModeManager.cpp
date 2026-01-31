#include "ModeManager.h"

static uint8_t _cachedMode = MODE_OTG;
static bool _modeInitialized = false;

uint8_t ModeManager::getMode() {
  if (!_modeInitialized) {
    Preferences prefs;
    if (prefs.begin("usb-ble", false)) {
      _cachedMode = prefs.getUChar("mode", MODE_OTG);
      // Validate mode is within range
      if (_cachedMode >= MODE_COUNT) {
        _cachedMode = MODE_OTG;
      }
      prefs.end();
    } else {
      _cachedMode = MODE_OTG;
    }
    _modeInitialized = true;
  }
  return _cachedMode;
}

void ModeManager::setMode(uint8_t mode) {
  if (mode >= MODE_COUNT) return;  // Validate

  Preferences prefs;
  if (prefs.begin("usb-ble", false)) {
    prefs.putUChar("mode", mode);
    prefs.end();
    _cachedMode = mode;
  }
}

void ModeManager::toggleMode() {
  uint8_t currentMode = getMode();
  // 3-way cycle: OTG -> WEB -> USB -> OTG
  uint8_t newMode = (currentMode + 1) % MODE_COUNT;
  setMode(newMode);
}

bool ModeManager::isOTGMode() {
  return getMode() == MODE_OTG;
}

bool ModeManager::isWebMode() {
  return getMode() == MODE_WEB;
}

bool ModeManager::isUSBMode() {
  return getMode() == MODE_USB;
}

const char* ModeManager::getModeName() {
  switch (getMode()) {
    case MODE_OTG: return "OTG";
    case MODE_WEB: return "WEB";
    case MODE_USB: return "USB";
    default: return "UNKNOWN";
  }
}
