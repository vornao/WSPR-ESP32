// Web interface: a control page at http://<hostname>.local/ and a small JSON API.
//
//   GET  /            the control page (web_page.h)
//   GET  /api/state   current state as JSON
//   GET  /api/history the transmission log, newest first
//   POST /api/cmd     cmd=<name>[&value=<n>]; names: freq, correction, drive, carrier, tones,
//                     beacon, interval, msgmode (0 alternate, 1 Type 1, 2 Type 3), next, cancel,
//                     reset (restore config.h defaults). Settings changes are saved to flash.
//                     Needs the header "X-Requested-With: wspr". Replies
//                     {"ok":bool,"error":"...","state":{...}} with the state after the command.
//
// Browser-borne attacks are refused: requests whose Host is not this device (DNS
// rebinding) and command posts without the custom header (cross-site request forgery,
// which can't set it). Optional HTTP basic auth keeps other people on the network out.
#pragma once

#include <WebServer.h>
#include <wspr_beacon.h>

#include "radio.h"
#include "station.h"
#include "time_sync.h"

class JsonWriter;

class WebUi {
 public:
  WebUi(Station &station, Radio &radio, wspr::Beacon &beacon, const TimeSync &time,
        const wspr::Message &message);

  // `password` empty = no login; otherwise HTTP basic auth with user "admin".
  // `checkHost` refuses requests addressed to any name but <hostname>, <hostname>.local
  // or the IP address; turn it off to reach the board through another DNS name or a proxy.
  void begin(const char *hostname, const char *password, bool checkHost);

  // Call often from loop(): serves pending requests, starts mDNS once WiFi is up.
  void service();

 private:
  bool admit(bool isCommand);  // sends the error reply and returns false if refused
  bool hostAllowed();
  void handlePage();
  void handleState();
  void handleHistory();
  void handleCommand();
  void reply(bool ok, const char *error);
  void writeState(JsonWriter &json);  // the state fields, into an open object
  void sendJson(int code, const JsonWriter &json);

  WebServer server_{80};
  Station &station_;
  Radio &radio_;
  wspr::Beacon &beacon_;
  const TimeSync &time_;
  const wspr::Message &message_;

  String hostname_;  // lower case
  const char *password_ = "";
  bool checkHost_ = true;
  bool mdnsTried_ = false;
};
