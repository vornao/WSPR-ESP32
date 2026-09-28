// WiFi connection and UTC time from NTP.
#pragma once

#include <stdint.h>
#include <time.h>

class TimeSync {
 public:
  // Starts connecting in the background; does not block.
  void begin(const char *ssid, const char *password, const char *ntpServer);

  // Call often from loop(): logs WiFi and NTP status changes.
  void service();

  bool synced() const;
  bool wifiConnected() const;

  static int64_t nowUtcUs();

 private:
  bool wifiWasConnected_ = false;
};

// Prints "YYYY-MM-DD hh:mm:ss UTC" to Serial.
void printUtc(time_t t);
