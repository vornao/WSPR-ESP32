#include "time_sync.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>

namespace {

// Set from the SNTP task.
volatile bool ntpSynced = false;
volatile bool ntpJustSynced = false;

void onNtpSync(struct timeval *) {
  ntpSynced = true;
  ntpJustSynced = true;
}

}  // namespace

void TimeSync::begin(const char *ssid, const char *password, const char *ntpServer) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  Serial.printf("WiFi: connecting to '%s'...\n", ssid);

  // Start SNTP after the network stack is up. It re-syncs on its own (default every hour).
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
  if (ntpJustSynced) {
    ntpJustSynced = false;
    Serial.print("NTP sync: ");
    printUtc(time(nullptr));
    Serial.println();
  }
}

bool TimeSync::synced() const {
  return ntpSynced;
}

bool TimeSync::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

int64_t TimeSync::nowUtcUs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000000LL + tv.tv_usec;
}

void printUtc(time_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d UTC", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
}
