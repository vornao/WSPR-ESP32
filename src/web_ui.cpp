#include "web_ui.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"
#include "web_page.h"

WebUi::WebUi(Station &station, Radio &radio, Beacon &beacon, const TimeSync &time,
             const wspr::Message &message)
    : station_(station), radio_(radio), beacon_(beacon), time_(time), message_(message) {}

void WebUi::begin(const char *mdnsName, const char *password) {
  mdnsName_ = mdnsName;
  password_ = password;

  server_.on("/", HTTP_GET, [this] { handlePage(); });
  server_.on("/api/state", HTTP_GET, [this] { handleState(); });
  server_.on("/api/history", HTTP_GET, [this] { handleHistory(); });
  server_.on("/api/cmd", HTTP_POST, [this] { handleCommand(); });
  server_.onNotFound([this] { server_.send(404, "text/plain", "not found"); });
  server_.begin();
}

void WebUi::service() {
  if (!mdnsStarted_ && time_.wifiConnected()) {
    mdnsStarted_ = MDNS.begin(mdnsName_);
    if (mdnsStarted_) MDNS.addService("http", "tcp", 80);
    Serial.printf("Web interface: http://%s.local/  or  http://%s/\n", mdnsName_,
                  WiFi.localIP().toString().c_str());
  }
  server_.handleClient();
}

bool WebUi::authorized() {
  if (password_[0] == '\0' || server_.authenticate("admin", password_)) return true;
  server_.requestAuthentication();
  return false;
}

void WebUi::handlePage() {
  if (!authorized()) return;
  server_.send(200, "text/html", WEB_PAGE);
}

void WebUi::handleState() {
  if (!authorized()) return;
  char json[1024];
  stateJson(json, sizeof(json));
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", json);
}

void WebUi::handleHistory() {
  if (!authorized()) return;
  static char json[2560];  // HISTORY_SIZE entries of ~100 chars
  size_t len = 0;
  len += snprintf(json + len, sizeof(json) - len, "[");
  for (int i = 0; i < beacon_.historyCount() && len < sizeof(json) - 128; i++) {
    const Beacon::TxRecord &r = beacon_.history(i);
    const char *st = r.status == Beacon::TxRecord::Status::Done      ? "done"
                     : r.status == Beacon::TxRecord::Status::Aborted ? "aborted"
                                                                     : "onair";
    len += snprintf(json + len, sizeof(json) - len,
                    "%s{\"t\":%lld,\"f\":%llu,\"part\":%d,\"msg\":\"%s\",\"st\":\"%s\"}",
                    i ? "," : "", (long long)r.startUtc, r.freqHz, r.part, message_.partName(r.part), st);
  }
  snprintf(json + len, sizeof(json) - len, "]");
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", json);
}

void WebUi::stateJson(char *out, size_t size) {
  int64_t nowMs = TimeSync::nowUtcUs() / 1000;
  snprintf(out, size,
           "{\"call\":\"%s\",\"locator\":\"%s\",\"dbm\":%d,\"messageOk\":%s,"
           "\"radioOk\":%s,\"centerHz\":%llu,\"effectiveHz\":%.2f,\"correctionPpb\":%ld,"
           "\"driveMa\":%d,\"beacon\":%s,\"everyN\":%d,\"carrier\":%s,\"pending\":%s,"
           "\"transmitting\":%s,\"symbol\":%d,\"symbols\":%d,\"txFreqHz\":%llu,"
           "\"wifi\":%s,\"rssi\":%d,\"synced\":%s,\"utcMs\":%lld,\"nextTx\":%lld,"
           "\"lastTx\":%lld,\"randomOffsetHz\":%d,\"msgMode\":%d,\"parts\":%d,"
           "\"part\":%d,\"nextPart\":%d,\"msg1\":\"%s\",\"msg2\":\"%s\"}",
           config::CALLSIGN, config::LOCATOR, config::POWER_DBM, message_.valid() ? "true" : "false",
           radio_.ok() ? "true" : "false", beacon_.centerHz(),
           radio_.effectiveHz((double)beacon_.centerHz()), (long)radio_.correctionPpb(),
           radio_.driveMa(), beacon_.enabled() ? "true" : "false", beacon_.everyNSlots(),
           station_.carrierOn() ? "true" : "false", beacon_.nextRequested() ? "true" : "false",
           beacon_.transmitting() ? "true" : "false", beacon_.currentSymbol() + 1, wspr::SYMBOL_COUNT,
           beacon_.txFreqHz(), time_.wifiConnected() ? "true" : "false", (int)WiFi.RSSI(),
           time_.synced() ? "true" : "false", time_.synced() ? nowMs : -1LL,
           (long long)beacon_.nextTxStart(), (long long)beacon_.lastTxStart(),
           config::TX_RANDOM_OFFSET_HZ, (int)beacon_.msgMode(), message_.parts(),
           beacon_.currentPart(), beacon_.nextPart(), message_.partName(0),
           message_.parts() > 1 ? message_.partName(1) : "");
}

void WebUi::handleCommand() {
  if (!authorized()) return;

  String cmd = server_.arg("cmd");
  String valueArg = server_.arg("value");
  char *end = nullptr;
  long long value = strtoll(valueArg.c_str(), &end, 10);
  bool hasValue = valueArg.length() > 0 && *end == '\0';

  Station::Result r = Station::Result::Invalid;
  if (cmd == "next") {
    r = station_.requestNextSlot();
  } else if (cmd == "cancel") {
    station_.cancel();
    r = Station::Result::Ok;
  } else if (cmd == "reset") {
    r = station_.restoreDefaults();
  } else if (!hasValue) {
    reply(false, "missing or bad value");
    return;
  } else if (cmd == "freq") {
    r = station_.setCenterHz(value);
  } else if (cmd == "correction") {
    r = station_.setCorrectionPpb(value);
  } else if (cmd == "drive") {
    r = station_.setDriveMa(value);
  } else if (cmd == "carrier") {
    r = station_.setCarrier(value != 0);
  } else if (cmd == "beacon") {
    r = station_.setBeaconEnabled(value != 0);
  } else if (cmd == "interval") {
    r = station_.setEveryNSlots(value);
  } else if (cmd == "msgmode") {
    r = station_.setMsgMode(value);
  } else {
    reply(false, "unknown command");
    return;
  }

  Serial.printf("[web] %s %s -> %s\n", cmd.c_str(), valueArg.c_str(), Station::describe(r));
  reply(r == Station::Result::Ok, Station::describe(r));
}

void WebUi::reply(bool ok, const char *error) {
  char state[1024];
  stateJson(state, sizeof(state));
  char json[1200];
  snprintf(json, sizeof(json), "{\"ok\":%s,\"error\":\"%s\",\"state\":%s}", ok ? "true" : "false",
           ok ? "" : error, state);
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(ok ? 200 : 400, "application/json", json);
}
