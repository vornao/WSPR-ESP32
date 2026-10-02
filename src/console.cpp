#include "console.h"

#include "utc_text.h"

using wspr::Beacon;

namespace {

constexpr size_t MAX_LINE = 64;

bool parseInt64(const String &s, int64_t &out) {
  String t = s;
  t.trim();
  if (t.length() == 0) return false;
  char *end = nullptr;
  long long v = strtoll(t.c_str(), &end, 10);
  if (*end != '\0') return false;
  out = v;
  return true;
}

const char *statusName(Beacon::TxRecord::Status s) {
  switch (s) {
    case Beacon::TxRecord::Status::Done: return "done";
    case Beacon::TxRecord::Status::Aborted: return "aborted";
    default: return "on air";
  }
}

}  // namespace

Console::Console(Station &station, Radio &radio, Beacon &beacon, const TimeSync &time,
                 const WifiLink &wifi, const wspr::Message &message, uint64_t stepHz)
    : station_(station), radio_(radio), beacon_(beacon), time_(time), wifi_(wifi), message_(message),
      stepHz_(stepHz) {}

void Console::service() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (station_.testMode() == Station::TestMode::Tones) {
      // Any key stops the tone sweep.
      station_.setTestMode(Station::TestMode::Off);
      while (Serial.available()) Serial.read();
      line_ = "";
      Serial.println("Tone sweep stopped.");
      break;
    }
    if (ch == '\r' || ch == '\n') {
      handle(line_);
      line_ = "";
    } else if ((ch == '+' || ch == '-') && line_.length() == 0) {
      handle(String(ch));  // step commands act immediately, no Enter needed
    } else if (line_.length() < MAX_LINE) {
      line_ += ch;
    }
  }
  logEvents();
}

// Logs transmissions starting and ending, and each tone of the tone sweep.
void Console::logEvents() {
  uint32_t changes = beacon_.changes();
  if (changes != seenChanges_) {
    seenChanges_ = changes;
    Beacon::TxRecord tx;
    if (beacon_.history(&tx, 1) == 1) {
      if (tx.status == Beacon::TxRecord::Status::OnAir) {
        Serial.printf("TX start %s | %llu Hz | %s dBm\n", utcText(tx.startUtc).s, tx.freqHz,
                      message_.partName(tx.part));
      } else {
        Serial.printf("TX %s\n", statusName(tx.status));
        printNextTx();
      }
    }
  }

  int tone = station_.testTone();
  if (tone != seenTone_) {
    seenTone_ = tone;
    if (tone >= 0) {
      uint64_t centerHz = beacon_.state().centerHz;
      uint64_t centiHz = wspr::toneCentiHz(centerHz, (uint8_t)tone);
      Serial.printf("Tone %d (+%.4f Hz, programmed +%.2f Hz):\n", tone, tone * wspr::TONE_SPACING_HZ,
                    (centiHz - centerHz * wspr::CENTI_HZ) / (double)wspr::CENTI_HZ);
      printFreq(centiHz / (double)wspr::CENTI_HZ);
    }
  }
}

// ---------- commands ----------

bool Console::report(Station::Result r, const char *usage) {
  switch (r) {
    case Station::Result::Ok:
      return true;
    case Station::Result::Invalid:
      Serial.printf("Usage: %s\n", usage);
      break;
    case Station::Result::Busy:
      Serial.println("Transmitting; 'x' to abort first.");
      break;
    case Station::Result::NoRadio:
      Serial.println("Si5351 not responding; check wiring and reset.");
      break;
  }
  return false;
}

void Console::handle(String line) {
  line.trim();
  if (line.length() == 0) return;
  char cmd = line.charAt(0);
  String arg = line.substring(1);
  arg.trim();
  int64_t v = 0;
  // Commands with an argument get an out-of-range value on a parse error, so Station rejects it.
  if (!parseInt64(arg, v)) v = INT64_MIN;

  using Mode = Station::TestMode;

  switch (cmd) {
    case 'b':
      if (report(station_.setBeaconEnabled(!beacon_.state().enabled), "")) {
        Serial.printf("Beacon: %s\n", beacon_.state().enabled ? "ON" : "OFF");
        printNextTx();
      }
      break;

    case 'n':
      if (!report(station_.requestNextSlot(), "")) break;
      Serial.print("TX requested for next slot");
      if (time_.synced()) Serial.printf(": %s", utcText(beacon_.state().nextTxUtc).s);
      else Serial.print(" (waiting for NTP)");
      Serial.println();
      break;

    case 'x': {
      bool wasPending = beacon_.state().requested;
      if (!station_.cancel()) Serial.println(wasPending ? "TX request cleared." : "Not transmitting.");
      break;
    }

    case 'i':
      if (report(station_.setEveryNSlots(v), "i <N>  (1 .. 30)")) {
        Serial.printf("TX every %d slot(s) (%d min)\n", (int)v, (int)v * 2);
        printNextTx();
      }
      break;

    case 'm':
      if (report(station_.setMsgMode(v),
                 "m <0|1|2>  (0 alternate T1/T3, 1 Type 1 only, 2 Type 3 only; "
                 "0 and 2 need a 6-char locator)")) {
        Serial.printf("Next TX sends: %s dBm\n", message_.partName(beacon_.state().nextPart));
      }
      break;

    case 'f':
      if (report(station_.setCenterHz(v), "f <Hz>  (8 kHz .. 160 MHz)")) printFreq((double)v);
      break;

    case '+':
    case '-': {
      int64_t hz = (int64_t)beacon_.state().centerHz + (cmd == '+' ? 1 : -1) * (int64_t)stepHz_;
      if (report(station_.setCenterHz(hz), "frequency out of range")) printFreq((double)hz);
      break;
    }

    case 's':
      if (v < 1 || v > 10000000) {
        Serial.println("Usage: s <Hz>  (1 .. 10000000)");
        break;
      }
      stepHz_ = (uint64_t)v;
      Serial.printf("Step: %llu Hz\n", stepHz_);
      break;

    case 'c':
      if (report(station_.setCorrectionPpb(v), "c <ppb>  (-1000000 .. 1000000)")) {
        Serial.printf("Correction: %ld ppb\n", (long)radio_.correctionPpb());
        printFreq((double)beacon_.state().centerHz);
      }
      break;

    case 'd':
      if (report(station_.setDriveMa(v), "d <2|4|6|8>")) Serial.printf("Drive: %d mA\n", radio_.driveMa());
      break;

    case 'o': {
      bool on = station_.testMode() != Mode::Carrier;
      if (report(station_.setTestMode(on ? Mode::Carrier : Mode::Off), "")) {
        Serial.printf("Test carrier: %s\n", on ? "ON (beacon paused)" : "OFF");
        if (on) printFreq((double)beacon_.state().centerHz);
      }
      break;
    }

    case 't':
      if (report(station_.setTestMode(Mode::Tones), ""))
        Serial.println("Tone sweep: WSPR tones 0-3 in turn, beacon paused. Press any key to stop.");
      break;

    case 'p':
      printState();
      break;

    case 'l':
      printHistory();
      break;

    case 'r':
      if (arg == "yes") {
        if (report(station_.restoreDefaults(), "")) {
          Serial.println("Settings restored to config.h defaults (flash copy erased).");
          printState();
        }
      } else {
        Serial.println("Type 'r yes' to restore the config.h defaults and erase the saved settings.");
      }
      break;

    case 'h':
    case '?':
      printHelp();
      break;

    default:
      Serial.printf("Unknown command '%c'. Type h for help.\n", cmd);
      break;
  }
}

// ---------- output ----------

void Console::printHistory() {
  Beacon::TxRecord log[Beacon::HISTORY_SIZE];
  int n = beacon_.history(log, Beacon::HISTORY_SIZE);
  if (n == 0) {
    Serial.println("No transmissions yet.");
    return;
  }
  Serial.println("Transmissions (newest first):");
  for (int i = 0; i < n; i++) {
    Serial.printf("  %s  %llu Hz  %s dBm  %s\n", utcText(log[i].startUtc).s, log[i].freqHz,
                  message_.partName(log[i].part), statusName(log[i].status));
  }
}

void Console::printNextTx() {
  int64_t next = beacon_.state().nextTxUtc;
  if (next > 0) Serial.printf("Next TX: %s\n", utcText(next).s);
  else Serial.printf("Next TX: %s\n", time_.synced() ? "none (beacon off)" : "waiting for NTP");
}

void Console::printFreq(double requestedHz) {
  double eff = radio_.effectiveHz(requestedHz);
  Serial.printf("  requested: %.2f Hz | effective (nominal xtal, corr %ld ppb): %.2f Hz (%+.2f Hz)\n",
                requestedHz, (long)radio_.correctionPpb(), eff, eff - requestedHz);
}

void Console::printState() {
  Beacon::State b = beacon_.state();
  const char *test[] = {"off", "carrier (beacon paused)", "tone sweep (beacon paused)"};

  Serial.println("State:");
  Serial.printf("  station:    %s %s %d dBm (%s)\n", message_.callsign(), message_.locator(),
                message_.powerDbm(), message_.valid() ? "ok" : "INVALID");
  Serial.printf("  freq:       %llu Hz (TX +/- %d Hz random)\n", b.centerHz, b.randomOffsetHz);
  Serial.printf("  step:       %llu Hz\n", stepHz_);
  Serial.printf("  correction: %ld ppb\n", (long)radio_.correctionPpb());
  Serial.printf("  drive:      %d mA\n", radio_.driveMa());
  Serial.printf("  beacon:     %s, every %d slot(s) (%d min)\n", b.enabled ? "ON" : "OFF", b.everyNSlots,
                b.everyNSlots * 2);
  Serial.printf("  test:       %s\n", test[(int)station_.testMode()]);
  Serial.printf("  WiFi:       %s\n", wifi_.connected() ? "connected" : "not connected");
  Serial.printf("  time:       %s\n", time_.synced() ? utcText(time(nullptr)).s : "not synced");
  if (b.transmitting) {
    Serial.printf("  TX:         symbol %d/%d at %llu Hz\n", b.symbol + 1, wspr::SYMBOL_COUNT, b.txFreqHz);
  } else {
    Serial.printf("  next TX:    %s\n", b.nextTxUtc > 0 ? utcText(b.nextTxUtc).s : "-");
  }
  Serial.printf("  chip:       %s\n", radio_.ready() ? "OK" : "NOT RESPONDING");
  printFreq((double)b.centerHz);
}

void Console::printHelp() {
  Serial.println("Commands:");
  Serial.println("  b            beacon on/off");
  Serial.println("  n            transmit in the next slot");
  Serial.println("  x            abort current transmission");
  Serial.println("  i <N>        transmit every N 2-minute slots");
  Serial.println("  m <0|1|2>    message: 0 alternate T1/T3, 1 Type 1 only, 2 Type 3 only");
  Serial.println("  f <Hz>       set centre frequency (e.g. f 14097100)");
  Serial.println("  + / -        step frequency up / down by current step");
  Serial.println("  s <Hz>       set step size (default 10 Hz)");
  Serial.println("  c <ppb>      set correction and re-apply it");
  Serial.println("  d <2|4|6|8>  set drive strength in mA");
  Serial.println("  o            test carrier on/off (pauses beacon)");
  Serial.println("  t            tone sweep: the 4 WSPR tones in turn, any key stops");
  Serial.println("  p            print current state");
  Serial.println("  l            transmission log (last 20)");
  Serial.println("  r yes        restore config.h defaults (settings are saved to flash on change)");
  Serial.println("  h            this help");
}
