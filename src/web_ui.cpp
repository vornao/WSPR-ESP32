#include "web_ui.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "json_writer.h"
#include "web_page.h"

using wspr::Beacon;

namespace {

constexpr const char *CSRF_HEADER = "X-Requested-With";
constexpr const char *CSRF_VALUE = "wspr";

// The page loads nothing from elsewhere; it only talks to this device and to wspr.live.
constexpr const char *PAGE_CSP =
    "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
    "connect-src 'self' https://db1.wspr.live; img-src 'self' data:; "
    "base-uri 'none'; form-action 'none'; frame-ancestors 'none'";

const char *statusName(Beacon::TxRecord::Status s) {
  switch (s) {
    case Beacon::TxRecord::Status::Done: return "done";
    case Beacon::TxRecord::Status::Aborted: return "aborted";
    default: return "onair";
  }
}

}  // namespace

WebUi::WebUi(Station &station, Radio &radio, Beacon &beacon, const TimeSync &time,
             const wspr::Message &message)
    : station_(station), radio_(radio), beacon_(beacon), time_(time), message_(message) {}

void WebUi::begin(const char *hostname, const char *password, bool checkHost) {
  hostname_ = hostname;
  hostname_.toLowerCase();
  password_ = password;
  checkHost_ = checkHost;

  static const char *headers[] = {CSRF_HEADER};
  server_.collectHeaders(headers, 1);
  server_.on("/", HTTP_GET, [this] { handlePage(); });
  server_.on("/api/state", HTTP_GET, [this] { handleState(); });
  server_.on("/api/history", HTTP_GET, [this] { handleHistory(); });
  server_.on("/api/cmd", HTTP_POST, [this] { handleCommand(); });
  server_.onNotFound([this] { server_.send(404, "text/plain", "not found"); });
  server_.begin();
}

void WebUi::service() {
  if (!mdnsTried_ && time_.wifiConnected()) {
    mdnsTried_ = true;
    if (MDNS.begin(hostname_.c_str())) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("Web interface: http://%s.local/  or  http://%s/\n", hostname_.c_str(),
                    WiFi.localIP().toString().c_str());
    } else {
      Serial.printf("mDNS failed; web interface at http://%s/\n", WiFi.localIP().toString().c_str());
    }
  }
  server_.handleClient();
}

// ---------- request checks ----------

bool WebUi::hostAllowed() {
  if (!checkHost_) return true;
  String host = server_.hostHeader();
  int colon = host.indexOf(':');
  if (colon >= 0) host.remove(colon);
  host.toLowerCase();
  return host.isEmpty() || host == hostname_ || host == hostname_ + ".local" ||
         host == WiFi.localIP().toString();
}

bool WebUi::admit(bool isCommand) {
  if (!hostAllowed()) {
    server_.send(403, "text/plain", "unknown host name");
    return false;
  }
  if (isCommand && server_.header(CSRF_HEADER) != CSRF_VALUE) {
    server_.send(403, "text/plain", "missing request header");
    return false;
  }
  if (password_[0] != '\0' && !server_.authenticate("admin", password_)) {
    server_.requestAuthentication();
    return false;
  }
  return true;
}

// ---------- handlers ----------

void WebUi::handlePage() {
  if (!admit(false)) return;
  server_.sendHeader("Content-Security-Policy", PAGE_CSP);
  server_.sendHeader("X-Frame-Options", "DENY");
  server_.sendHeader("X-Content-Type-Options", "nosniff");
  server_.send_P(200, "text/html", WEB_PAGE, sizeof(WEB_PAGE) - 1);  // straight from flash
}

void WebUi::handleState() {
  if (!admit(false)) return;
  static char buf[1536];
  JsonWriter json(buf, sizeof buf);
  json.beginObject();
  writeState(json);
  json.endObject();
  sendJson(200, json);
}

void WebUi::handleHistory() {
  if (!admit(false)) return;
  Beacon::TxRecord log[Beacon::HISTORY_SIZE];
  int n = beacon_.history(log, Beacon::HISTORY_SIZE);

  static char buf[3072];  // ~110 bytes per entry
  JsonWriter json(buf, sizeof buf);
  json.beginArray();
  for (int i = 0; i < n; i++) {
    json.beginObject();
    json.field("t", log[i].startUtc);
    json.field("f", log[i].freqHz);
    json.field("part", log[i].part);
    json.field("msg", message_.partName(log[i].part));
    json.field("st", statusName(log[i].status));
    json.endObject();
  }
  json.endArray();
  sendJson(200, json);
}

void WebUi::handleCommand() {
  if (!admit(true)) return;

  String cmd = server_.arg("cmd");
  String valueArg = server_.arg("value");
  char *end = nullptr;
  long long value = strtoll(valueArg.c_str(), &end, 10);
  bool hasValue = valueArg.length() > 0 && *end == '\0';

  using Mode = Station::TestMode;
  Station::Result r;
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
    r = station_.setTestMode(value ? Mode::Carrier : Mode::Off);
  } else if (cmd == "tones") {
    r = station_.setTestMode(value ? Mode::Tones : Mode::Off);
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

// ---------- JSON ----------

void WebUi::reply(bool ok, const char *error) {
  static char buf[1664];
  JsonWriter json(buf, sizeof buf);
  json.beginObject();
  json.field("ok", ok);
  json.field("error", ok ? "" : error);
  json.beginObject("state");
  writeState(json);
  json.endObject();
  sendJson(ok ? 200 : 400, json);
}

// The fields of the state object, into the object `json` has open.
void WebUi::writeState(JsonWriter &json) {
  Beacon::State b = beacon_.state();
  bool synced = time_.synced();
  double centerHz = (double)b.centerHz;

  json.field("call", message_.callsign());
  json.field("locator", message_.locator());
  json.field("dbm", message_.powerDbm());
  json.field("messageOk", message_.valid());
  json.field("parts", message_.parts());
  json.field("msg1", message_.partName(wspr::Message::TYPE1));
  json.field("msg2", message_.partName(wspr::Message::TYPE3));

  json.field("radioOk", radio_.ready());
  json.field("centerHz", b.centerHz);
  json.field("effectiveHz", radio_.effectiveHz(centerHz), 2);
  json.field("correctionPpb", radio_.correctionPpb());
  json.field("driveMa", radio_.driveMa());
  json.field("carrier", station_.testMode() == Station::TestMode::Carrier);
  json.field("tones", station_.testMode() == Station::TestMode::Tones);
  json.field("tone", station_.testTone());

  json.field("beacon", b.enabled);
  json.field("everyN", b.everyNSlots);
  json.field("randomOffsetHz", b.randomOffsetHz);
  json.field("msgMode", (int)b.msgMode);
  json.field("pending", b.requested);
  json.field("transmitting", b.transmitting);
  json.field("symbol", b.symbol + 1);
  json.field("symbols", wspr::SYMBOL_COUNT);
  json.field("txFreqHz", b.txFreqHz);
  json.field("part", b.part);
  json.field("nextPart", b.nextPart);
  json.field("nextTx", b.nextTxUtc);
  json.field("lastTx", b.lastTxUtc);

  json.field("wifi", time_.wifiConnected());
  json.field("rssi", time_.rssi());
  json.field("synced", synced);
  json.field("utcMs", synced ? time_.utcUs() / 1000 : -1LL);
}

void WebUi::sendJson(int code, const JsonWriter &json) {
  server_.sendHeader("Cache-Control", "no-store");
  if (json.ok()) server_.send(code, "application/json", json.c_str());
  else server_.send(500, "text/plain", "response too large");
}
