// Firmware updates over WiFi (ArduinoOTA / espota).
//   pio run -e esp32c3-ota -t upload
// Any transmission is aborted and the beacon paused while an update is received.
#pragma once

#include <functional>

class Ota {
 public:
  // `password` empty = no password (anyone on the LAN can flash the board).
  // `onStart` runs when an update begins (stop transmitting there),
  // `onFail` if it fails (the old firmware keeps running).
  void begin(const char *hostname, const char *password, std::function<void()> onStart,
             std::function<void()> onFail);

  // Call often from loop(): starts listening once WiFi is up, then handles updates.
  void service(bool wifiConnected);

  bool updating() const { return updating_; }

 private:
  const char *hostname_ = "wspr";
  const char *password_ = "";
  std::function<void()> onStart_;
  std::function<void()> onFail_;
  bool started_ = false;
  bool updating_ = false;
};
