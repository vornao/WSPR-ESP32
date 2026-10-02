#include "wifi_link.h"

#include <WiFi.h>

void WifiLink::begin(const char *ssid, const char *password) {
  WiFi.mode(WIFI_STA);
  // Modem sleep delays incoming packets and misses mDNS multicast, which makes the web
  // interface slow to answer and wspr.local hard to resolve.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // the C3 Super Mini needs reduced TX power to work reliably
  Serial.printf("WiFi: connecting to '%s'...\n", ssid);
}

void WifiLink::service() {
  bool now = connected();
  if (now == wasConnected_) return;
  wasConnected_ = now;
  if (now) Serial.printf("WiFi connected, IP %s\n", localIp().toString().c_str());
  else Serial.println("WiFi disconnected (will retry)");
}

bool WifiLink::connected() const {
  return WiFi.status() == WL_CONNECTED;
}

int WifiLink::rssi() const {
  return connected() ? WiFi.RSSI() : 0;
}

IPAddress WifiLink::localIp() const {
  return WiFi.localIP();
}
