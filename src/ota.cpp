#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

void Ota::begin(const char *hostname, const char *password, std::function<void()> onStart,
                std::function<void()> onFail) {
  hostname_ = hostname;
  password_ = password;
  onStart_ = onStart;
  onFail_ = onFail;
  if (password_[0] == '\0') Serial.println("OTA: off (set OTA_PASSWORD in secrets.h to enable it)");
}

void Ota::service(bool wifiConnected) {
  if (password_[0] == '\0') return;
  if (!started_) {
    if (!wifiConnected) return;
    ArduinoOTA.setHostname(hostname_);
    ArduinoOTA.setMdnsEnabled(false);  // the web interface already announces <hostname>.local
    ArduinoOTA.setPassword(password_);

    ArduinoOTA.onStart([this] {
      if (onStart_) onStart_();
      Serial.println("OTA: update started, beacon on hold");
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
      static unsigned int lastDecile = 11;
      unsigned int decile = total ? done * 10 / total : 0;
      if (decile != lastDecile) Serial.printf("OTA: %u%%\n", decile * 10);
      lastDecile = decile;
    });
    ArduinoOTA.onEnd([] { Serial.println("OTA: done, rebooting"); });
    ArduinoOTA.onError([this](ota_error_t e) {
      Serial.printf("OTA: failed (error %u), keeping current firmware\n", (unsigned)e);
      if (onFail_) onFail_();
    });
    ArduinoOTA.begin();
    started_ = true;
    Serial.printf("OTA: ready on %s.local:3232 (password protected)\n", hostname_);
  }
  ArduinoOTA.handle();
}
