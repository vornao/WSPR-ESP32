#include "settings.h"

#include <Preferences.h>

#include "config.h"

namespace settings {

namespace {

constexpr const char *NS = "wspr";
constexpr uint8_t VERSION = 1;  // bump if the meaning of a key changes

Settings stored;  // what is in flash (or defaults if nothing is)
bool storedValid = false;

template <typename T>
T validOr(T value, T fallback, bool (*valid)(int64_t)) {
  return valid((int64_t)value) ? value : fallback;
}

}  // namespace

Settings defaults() {
  Settings s;
  s.centerHz = config::CENTER_FREQ_HZ;
  s.correctionPpb = config::CORRECTION_PPB;
  s.driveMa = (uint8_t)config::DRIVE_MA;
  s.everyNSlots = (uint8_t)config::TX_EVERY_N_SLOTS;
  s.msgMode = 0;  // alternate Type 1 / Type 3 (falls back to Type 1 with a 4-char locator)
  s.beaconOn = true;
  return s;
}

Settings load(bool *fromFlash) {
  Settings d = defaults();
  Settings s = d;
  bool found = false;
  Preferences p;
  if (p.begin(NS, true)) {
    if (p.getUChar("ver", 0) == VERSION) {
      found = true;
      s.centerHz = validOr<uint64_t>(p.getULong64("center", d.centerHz), d.centerHz, limits::centerHz);
      s.correctionPpb = validOr<int32_t>(p.getInt("corr", d.correctionPpb), d.correctionPpb, limits::correctionPpb);
      s.driveMa = validOr<uint8_t>(p.getUChar("drive", d.driveMa), d.driveMa, limits::driveMa);
      s.everyNSlots = validOr<uint8_t>(p.getUChar("everyN", d.everyNSlots), d.everyNSlots, limits::everyNSlots);
      s.msgMode = validOr<uint8_t>(p.getUChar("mode", d.msgMode), d.msgMode, limits::msgMode);
      s.beaconOn = p.getBool("beacon", d.beaconOn);
    }
    p.end();
  }
  stored = s;
  storedValid = found;
  if (fromFlash) *fromFlash = found;
  return s;
}

bool save(const Settings &s) {
  if (storedValid && s == stored) return false;
  Preferences p;
  if (!p.begin(NS, false)) return false;
  p.putUChar("ver", VERSION);
  p.putULong64("center", s.centerHz);
  p.putInt("corr", s.correctionPpb);
  p.putUChar("drive", s.driveMa);
  p.putUChar("everyN", s.everyNSlots);
  p.putUChar("mode", s.msgMode);
  p.putBool("beacon", s.beaconOn);
  p.end();
  stored = s;
  storedValid = true;
  return true;
}

void clear() {
  Preferences p;
  if (p.begin(NS, false)) {
    p.clear();
    p.end();
  }
  stored = defaults();
  storedValid = false;
}

}  // namespace settings
