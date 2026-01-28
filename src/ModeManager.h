#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

class ModeManager {
public:
  static uint8_t getMode();
  static void setMode(uint8_t mode);
  static void toggleMode();
  static bool isOTGMode();
  static bool isWebMode();
};

#endif
