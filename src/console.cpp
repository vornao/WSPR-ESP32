#include "console.h"

#include <si5351.h>

#include "config.h"

namespace {

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

}  // namespace

Console::Console(Station &station, Radio &radio, Beacon &beacon, const TimeSync &time,
                 const wspr::Message &message, StatusLed &led)
    : station_(station), radio_(radio), beacon_(beacon), time_(time), message_(message), led_(led),
      stepHz_(config::STEP_HZ) {}

void Console::poll() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r' || ch == '\n') {
      handle(line_);
      line_ = "";
    } else if ((ch == '+' || ch == '-') && line_.length() == 0) {
      // step commands act immediately, no Enter needed
      handle(String(ch));
    } else if (line_.length() < 64) {
      line_ += ch;
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
      Serial.println("Si5351 not responding; only 'h' and 'p' are available. Check wiring and reset.");
      break;
  }
  return false;
}

void Console::handle(String line) {
  line.trim();
  if (line.length() == 0) return;
  char cmd = line.charAt(0);
  String arg = line.substring(1);
  int64_t v = 0;
  // Commands with an argument get an out-of-range value on a parse error, so Station rejects it.
  if (!parseInt64(arg, v)) v = INT64_MIN;

  switch (cmd) {
    case 'b':
      if (report(station_.setBeaconEnabled(!beacon_.enabled()), "")) {
        Serial.printf("Beacon: %s\n", beacon_.enabled() ? "ON" : "OFF");
        printNextTx();
      }
      break;

    case 'n':
      if (!report(station_.requestNextSlot(), "")) break;
      Serial.print("TX requested for next slot");
      if (time_.synced()) {
        Serial.print(": ");
        printUtc((time_t)beacon_.nextTxStart());
      } else {
        Serial.print(" (waiting for NTP)");
      }
      Serial.println();
      break;

    case 'x': {
      bool wasPending = beacon_.nextRequested();
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
      if (report(station_.setMsgMode(v), "m <0|1|2>  (0 alternate T1/T3, 1 Type 1 only, 2 Type 3 only; needs a 6-char locator for 0 and 2)")) {
        Serial.printf("Next TX sends: %s dBm\n", message_.partName(beacon_.nextPart()));
      }
      break;

    case 'f':
      if (report(station_.setCenterHz(v), "f <Hz>  (8 kHz .. 160 MHz)")) printFreq((double)v);
      break;

    case '+':
    case '-': {
      int64_t hz = (int64_t)beacon_.centerHz() + (cmd == '+' ? 1 : -1) * (int64_t)stepHz_;
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
        printFreq((double)beacon_.centerHz());
      }
      break;

    case 'd':
      if (report(station_.setDriveMa(v), "d <2|4|6|8>")) Serial.printf("Drive: %d mA\n", radio_.driveMa());
      break;

    case 'o':
      if (report(station_.setCarrier(!station_.carrierOn()), "")) {
        Serial.printf("Test carrier: %s\n", station_.carrierOn() ? "ON (beacon paused)" : "OFF");
        if (station_.carrierOn()) printFreq((double)beacon_.centerHz());
      }
      break;

    case 't':
      if (!radio_.ok()) report(Station::Result::NoRadio, "");
      else if (beacon_.transmitting()) report(Station::Result::Busy, "");
      else toneTest();
      break;

    case 'p':
      printState();
      break;

    case 'l':
      printHistory();
      break;

    case 'r':
      if (arg.length() > 0 && arg.indexOf("yes") >= 0) {
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

// Steps through the 4 WSPR tones, 2 s each, until a key is pressed.
// Checks that the fine frequency resolution works.
void Console::toneTest() {
  Serial.println("Tone test: stepping through WSPR tones 0-3. Press any key to stop.");
  while (Serial.available()) Serial.read();
  radio_.setOutput(true);
  led_.setMode(StatusLed::Mode::Blink);

  uint64_t centerHz = beacon_.centerHz();
  bool stop = false;
  while (!stop) {
    for (int tone = 0; tone < 4 && !stop; tone++) {
      uint64_t centiHz = wspr::toneCentiHz(centerHz, tone);
      radio_.setFrequencyCentiHz(centiHz);
      Serial.printf("Tone %d (+%.4f Hz, programmed +%.2f Hz):\n", tone, tone * wspr::TONE_SPACING_HZ,
                    (centiHz - centerHz * SI5351_FREQ_MULT) / (double)SI5351_FREQ_MULT);
      printFreq(centiHz / (double)SI5351_FREQ_MULT);

      uint32_t start = millis();
      while (millis() - start < config::TONE_DWELL_MS) {
        led_.update();
        if (Serial.available()) { stop = true; break; }
        delay(5);
      }
    }
  }
  while (Serial.available()) Serial.read();
  Serial.println("Tone test stopped.");
  radio_.setFrequencyCentiHz(centerHz * SI5351_FREQ_MULT);
  radio_.setOutput(station_.carrierOn());
}

// ---------- output ----------

void Console::printHistory() {
  int n = beacon_.historyCount();
  if (n == 0) {
    Serial.println("No transmissions yet.");
    return;
  }
  Serial.println("Transmissions (newest first):");
  for (int i = 0; i < n; i++) {
    const Beacon::TxRecord &r = beacon_.history(i);
    Serial.print("  ");
    printUtc((time_t)r.startUtc);
    const char *st = r.status == Beacon::TxRecord::Status::Done      ? "done"
                     : r.status == Beacon::TxRecord::Status::Aborted ? "aborted"
                                                                     : "on air";
    Serial.printf("  %llu Hz  %s dBm  %s\n", r.freqHz, message_.partName(r.part), st);
  }
}

void Console::printNextTx() {
  int64_t next = beacon_.nextTxStart();
  Serial.print("Next TX: ");
  if (next > 0) printUtc((time_t)next);
  else Serial.print(time_.synced() ? "none (beacon off)" : "waiting for NTP");
  Serial.println();
}

void Console::printFreq(double requestedHz) {
  double eff = radio_.effectiveHz(requestedHz);
  Serial.printf("  requested: %.2f Hz | effective (nominal xtal, corr %ld ppb): %.2f Hz (%+.2f Hz)\n",
                requestedHz, (long)radio_.correctionPpb(), eff, eff - requestedHz);
}

void Console::printState() {
  Serial.println("State:");
  Serial.printf("  station:    %s %s %d dBm (%s)\n", config::CALLSIGN, config::LOCATOR,
                config::POWER_DBM, message_.valid() ? "ok" : "INVALID");
  Serial.printf("  freq:       %llu Hz (TX +/- %d Hz random)\n", beacon_.centerHz(),
                config::TX_RANDOM_OFFSET_HZ);
  Serial.printf("  step:       %llu Hz\n", stepHz_);
  Serial.printf("  correction: %ld ppb\n", (long)radio_.correctionPpb());
  Serial.printf("  drive:      %d mA\n", radio_.driveMa());
  Serial.printf("  beacon:     %s, every %d slot(s) (%d min)\n", beacon_.enabled() ? "ON" : "OFF",
                beacon_.everyNSlots(), beacon_.everyNSlots() * 2);
  Serial.printf("  carrier:    %s\n", station_.carrierOn() ? "ON (beacon paused)" : "off");
  Serial.printf("  WiFi:       %s\n", time_.wifiConnected() ? "connected" : "not connected");

  Serial.print("  time:       ");
  if (time_.synced()) printUtc(time(nullptr));
  else Serial.print("not synced");
  Serial.println();

  if (beacon_.transmitting()) {
    Serial.printf("  TX:         symbol %d/%d at %llu Hz\n", beacon_.currentSymbol() + 1,
                  wspr::SYMBOL_COUNT, beacon_.txFreqHz());
  } else {
    int64_t next = beacon_.nextTxStart();
    Serial.print("  next TX:    ");
    if (next > 0) printUtc((time_t)next);
    else Serial.print("-");
    Serial.println();
  }

  Serial.printf("  chip:       %s\n", radio_.ok() ? "OK" : "NOT RESPONDING");
  printFreq((double)beacon_.centerHz());
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
  Serial.println("  t            tone test: 4 WSPR tones, 2 s each, any key stops");
  Serial.println("  p            print current state");
  Serial.println("  l            transmission log (last 20)");
  Serial.println("  r yes        restore config.h defaults (settings are saved to flash on change)");
  Serial.println("  h            this help");
}
