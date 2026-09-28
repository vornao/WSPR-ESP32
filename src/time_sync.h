// WiFi connection and UTC time from NTP.
#pragma once

#include <stdint.h>

#include <wspr_beacon.h>

class TimeSync : public wspr::Clock {
 public:
  // The ESP32 clock drifts by up to about 2 s a day. Past this long without a successful
  // NTP sync the time no longer counts as synced, and the beacon stops transmitting.
  static constexpr uint32_t MAX_SYNC_AGE_S = 12 * 3600;

  // Starts connecting in the background; does not block.
  void begin(const char *ssid, const char *password, const char *ntpServer);

  // Call often from loop(): logs WiFi and NTP status changes.
  void service();

  // wspr::Clock
  bool synced() const override;
  int64_t utcUs() const override;

  bool wifiConnected() const;
  int rssi() const;

 private:
  bool wifiWasConnected_ = false;
  bool wasSynced_ = false;
};
