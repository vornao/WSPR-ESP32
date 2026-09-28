// Onboard status LED: off, blinking or solid.
#pragma once

#include <stdint.h>

class StatusLed {
 public:
  enum class Mode { Off, Blink, Solid };

  void begin(int pin, bool activeLow, uint32_t blinkMs);
  void setMode(Mode mode);

  // Call often from loop() to drive the blink.
  void update();

 private:
  void write(bool on);

  int pin_ = -1;
  bool activeLow_ = false;
  uint32_t blinkMs_ = 250;
  Mode mode_ = Mode::Off;
  bool lit_ = false;
  uint32_t lastToggleMs_ = 0;
};
