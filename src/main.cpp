#include "Bridge.h"
#include "Config.h"
#include "ModeManager.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 USB to BLE Keyboard Bridge           ║");
  Serial.println("║  Supports keyboard + multi-device              ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println();

  if (LED_FEEDBACK_PIN >= 0) {
    pinMode(LED_FEEDBACK_PIN, OUTPUT);
    digitalWrite(LED_FEEDBACK_PIN, LOW);
  }

  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  Bridge::begin();

  Serial.println();
  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║  READY - Connect USB devices via hub           ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println();
}

void loop() {
  Bridge::loop();
  
  static String serialInput = "";
  static unsigned long lastButtonCheck = 0;
  static bool lastButtonState = HIGH;
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialInput.trim();
      serialInput.toUpperCase();

      if (serialInput == "TOGLE_MODE" || serialInput == "TOGGLE_MODE") {
        Serial.println("[Mode] Toggling mode via serial command...");
        Bridge::switchMode();  // Uses proper cleanup before restart
      }

      serialInput = "";
    } else {
      serialInput += c;
    }
  }
  
  if (millis() - lastButtonCheck > 50) {
    lastButtonCheck = millis();
    bool currentButtonState = digitalRead(MODE_BUTTON_PIN);
    
    if (lastButtonState == HIGH && currentButtonState == LOW) {
      delay(50);
      if (digitalRead(MODE_BUTTON_PIN) == LOW) {
        Serial.println("[Mode] Button pressed - toggling mode...");
        Bridge::switchMode();  // Uses proper cleanup before restart
      }
    }
    
    lastButtonState = currentButtonState;
  }
  
  delay(10);
}
