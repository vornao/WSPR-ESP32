#include "time_sync.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>

#include <atomic>

#include "utc_text.h"

namespace {

// Written by the SNTP task: seconds since boot of the last sync, 0 = never.
std::atomic<uint32_t> lastSyncS{0};
std::atomic<bool> justSynced{false};

uint32_t uptimeS() { return (uint32_t)(esp_timer_get_time() / 1000000); }

void onNtpSync(struct timeval *) {
  uint32_t now = uptimeS();
  lastSyncS = now > 0 ? now : 1;
  justSynced = true;
}

}  // namespace

void TimeSync::begin(const char *ssid, const char *password, const char *ntpServer) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  Serial.printf("WiFi: connecting to '%s'...\n", ssid);

  // SNTP re-syncs on its own (every hour by default).
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime("UTC0", ntpServer);
}

void TimeSync::service() {
  bool connected = wifiConnected();
  if (connected != wifiWasConnected_) {
    wifiWasConnected_ = connected;
    if (connected) Serial.printf("WiFi connected, IP %s\n", WiFi.localIP().toString().c_str());
    else Serial.println("WiFi disconnected (will retry)");
  }
  if (justSynced.exchange(false)) {
    Serial.printf("NTP sync: %s\n", utcText(time(nullptr)).s);
  }
  bool isSynced = synced();
  if (wasSynced_ && !isSynced) Serial.println("NTP: no sync for too long, time is stale; beacon on hold");
  wasSynced_ = isSynced;
}

bool TimeSync::synced() const {
  uint32_t last = lastSyncS;
  return last != 0 && uptimeS() - last < MAX_SYNC_AGE_S;
}

int64_t TimeSync::utcUs() const {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

bool TimeSync::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

int TimeSync::rssi() const {
  return wifiConnected() ? WiFi.RSSI() : 0;
}
