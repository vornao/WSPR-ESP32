// Web interface: serves a control page at http://<mdns-name>.local/ and a small JSON API.
//
//   GET  /            the control page (web_page.h)
//   GET  /api/state   current state as JSON
//   GET  /api/history the transmission log, newest first
//   POST /api/cmd     cmd=<name>[&value=<n>]; names: freq, correction, drive, carrier,
//                     beacon, interval, msgmode (0 alternate, 1 Type 1, 2 Type 3), next, cancel,
//                     reset (restore config.h defaults). Settings changes are saved to flash.
//                     Replies {"ok":bool,"error":"...","state":{...}} with the state after the command.
#pragma once

#include <WebServer.h>

#include "beacon.h"
#include "radio.h"
#include "station.h"
#include "time_sync.h"
#include "wspr.h"

class WebUi {
 public:
  WebUi(Station &station, Radio &radio, Beacon &beacon, const TimeSync &time,
        const wspr::Message &message);

  // `password` empty = no login. Otherwise HTTP basic auth with user "admin".
  void begin(const char *mdnsName, const char *password);

  // Call often from loop(): serves pending requests, starts mDNS once WiFi is up.
  void service();

 private:
  bool authorized();
  void handlePage();
  void handleState();
  void handleHistory();
  void handleCommand();
  void reply(bool ok, const char *error);

  // Writes the current state as a JSON object into `out`.
  void stateJson(char *out, size_t size);

  WebServer server_{80};
  Station &station_;
  Radio &radio_;
  Beacon &beacon_;
  const TimeSync &time_;
  const wspr::Message &message_;

  const char *mdnsName_ = "wspr";
  const char *password_ = "";
  bool mdnsStarted_ = false;
};
