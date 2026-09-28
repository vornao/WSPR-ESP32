#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

void Ota::begin(const char *hostname, const char *password, std::function<void()> onStart,
                std::function<void()> onFail) {
  hostname_ = hostname;
  password_ = password;
  onStart_ = onStart;
  onFail_ = onFail;
}

void Ota::service(bool wifiConnected) {
  if (!started_) {
    if (!wifiConnected) return;
    ArduinoOTA.setHostname(hostname_);
    ArduinoOTA.setMdnsEnabled(false);  // the web interface already announces <hostname>.local
    if (password_[0] != '\0') ArduinoOTA.setPassword(password_);

    ArduinoOTA.onStart([this] {
      updating_ = true;
      if (onStart_) onStart_();
      Serial.println("OTA: update started, beacon paused");
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
      static unsigned int lastPct = 101;
      unsigned int pct = total ? done * 100 / total : 0;
      if (pct / 10 != lastPct / 10) Serial.printf("OTA: %u%%\n", pct);
      lastPct = pct;
    });
    ArduinoOTA.onEnd([] { Serial.println("OTA: done, rebooting"); });
    ArduinoOTA.onError([this](ota_error_t e) {
      updating_ = false;
      Serial.printf("OTA: failed (error %u), keeping current firmware\n", (unsigned)e);
      if (onFail_) onFail_();
    });
    ArduinoOTA.begin();
    started_ = true;
    Serial.printf("OTA: ready on %s.local:3232%s\n", hostname_,
                  password_[0] ? " (password protected)" : " (NO PASSWORD - set OTA_PASSWORD in secrets.h)");
  }
  ArduinoOTA.handle();
}
