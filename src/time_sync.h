// UTC time from NTP, as the beacon's wspr::Clock. Needs the network: begin() once WiFi is up.
#pragma once

#include <stdint.h>

#include <wspr_beacon.h>

class TimeSync : public wspr::Clock {
 public:
  // The ESP32 clock drifts by up to about 2 s a day. Past this long without a successful
  // NTP sync the time no longer counts as synced, and the beacon stops transmitting.
  static constexpr uint32_t MAX_SYNC_AGE_S = 12 * 3600;

  // Starts syncing in the background (and re-syncing on its own); does not block.
  void begin(const char *ntpServer);
  bool started() const { return started_; }

  // Call often from loop(): logs syncs, and the time going stale.
  void service();

  // wspr::Clock
  bool synced() const override;
  int64_t utcUs() const override;

 private:
  bool started_ = false;
  bool wasSynced_ = false;
};
