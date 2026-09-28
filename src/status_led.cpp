#include "status_led.h"

#include <Arduino.h>

void StatusLed::begin(int pin, bool activeLow, uint32_t blinkMs) {
  pin_ = pin;
  activeLow_ = activeLow;
  blinkMs_ = blinkMs;
  pinMode(pin_, OUTPUT);
  write(false);
}

void StatusLed::setMode(Mode mode) {
  if (mode == mode_) return;
  mode_ = mode;
  write(mode == Mode::Solid);
  lastToggleMs_ = millis();
}

void StatusLed::update() {
  if (mode_ != Mode::Blink) return;
  if (millis() - lastToggleMs_ >= blinkMs_) {
    lastToggleMs_ = millis();
    write(!lit_);
  }
}

void StatusLed::write(bool on) {
  lit_ = on;
  digitalWrite(pin_, (on ^ activeLow_) ? HIGH : LOW);
}
