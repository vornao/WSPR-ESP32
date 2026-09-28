// Firmware updates over WiFi (ArduinoOTA / espota):
//   pio run -e esp32c3-ota -t upload
// Only enabled with a password: otherwise anyone on the network could flash firmware onto
// a board wired to a transmitter.
#pragma once

#include <functional>

class Ota {
 public:
  // `password` empty = OTA stays off. `onStart` runs when an update begins (get off the
  // air there), `onFail` if it fails (the old firmware keeps running).
  void begin(const char *hostname, const char *password, std::function<void()> onStart,
             std::function<void()> onFail);

  // Call often from loop(): starts listening once WiFi is up, then handles updates.
  void service(bool wifiConnected);

 private:
  const char *hostname_ = "";
  const char *password_ = "";
  std::function<void()> onStart_;
  std::function<void()> onFail_;
  bool started_ = false;
};
