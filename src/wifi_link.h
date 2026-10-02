// WiFi station link, shared by every module that needs the network (NTP, web UI, OTA).
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

class WifiLink {
 public:
  // Starts connecting in the background and keeps reconnecting; does not block.
  void begin(const char *ssid, const char *password);

  // Call often from loop(): logs connects and disconnects.
  void service();

  bool connected() const;
  int rssi() const;  // dBm, 0 while disconnected
  IPAddress localIp() const;

 private:
  bool wasConnected_ = false;
};
