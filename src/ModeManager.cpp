#include "ModeManager.h"

static uint8_t _cachedMode = MODE_OTG;
static bool _modeInitialized = false;

uint8_t ModeManager::getMode() {
  if (!_modeInitialized) {
    Preferences prefs;
    if (prefs.begin("usb-ble", false)) {
      _cachedMode = prefs.getUChar("mode", MODE_OTG);
      prefs.end();
    } else {
      _cachedMode = MODE_OTG;
    }
    _modeInitialized = true;
  }
  return _cachedMode;
}

void ModeManager::setMode(uint8_t mode) {
  Preferences prefs;
  if (prefs.begin("usb-ble", false)) {
    prefs.putUChar("mode", mode);
    prefs.end();
    _cachedMode = mode;
  }
}

void ModeManager::toggleMode() {
  uint8_t currentMode = getMode();
  uint8_t newMode = (currentMode == MODE_OTG) ? MODE_WEB : MODE_OTG;
  setMode(newMode);
}

bool ModeManager::isOTGMode() {
  return getMode() == MODE_OTG;
}

bool ModeManager::isWebMode() {
  return getMode() == MODE_WEB;
}
