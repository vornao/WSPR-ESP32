// Runtime settings kept in flash (NVS), so changes made from the console or the web page
// survive a restart. Defaults come from config.h; "restore defaults" erases the flash copy.
#pragma once

#include <stdint.h>

struct Settings {
  uint64_t centerHz;
  int32_t correctionPpb;
  uint8_t driveMa;
  uint8_t everyNSlots;
  uint8_t msgMode;  // wspr::Beacon::MsgMode
  bool beaconOn;

  bool operator==(const Settings &o) const {
    return centerHz == o.centerHz && correctionPpb == o.correctionPpb && driveMa == o.driveMa &&
           everyNSlots == o.everyNSlots && msgMode == o.msgMode && beaconOn == o.beaconOn;
  }
  bool operator!=(const Settings &o) const { return !(*this == o); }
};

// The accepted range of each setting, shared by the console, the web API and load().
namespace limits {

constexpr int64_t MIN_CENTER_HZ = 8000;       // Si5351 output range
constexpr int64_t MAX_CENTER_HZ = 160000000;
constexpr int64_t MAX_CORRECTION_PPB = 1000000;
constexpr int64_t MAX_EVERY_N_SLOTS = 30;

constexpr bool centerHz(int64_t hz) { return hz >= MIN_CENTER_HZ && hz <= MAX_CENTER_HZ; }
constexpr bool correctionPpb(int64_t ppb) { return ppb >= -MAX_CORRECTION_PPB && ppb <= MAX_CORRECTION_PPB; }
constexpr bool driveMa(int64_t ma) { return ma == 2 || ma == 4 || ma == 6 || ma == 8; }
constexpr bool everyNSlots(int64_t n) { return n >= 1 && n <= MAX_EVERY_N_SLOTS; }
constexpr bool msgMode(int64_t mode) { return mode >= 0 && mode <= 2; }

}  // namespace limits

namespace settings {

// The values from config.h.
Settings defaults();

// Stored values; anything missing or out of range gets its default.
// `fromFlash` tells whether a stored copy was found.
Settings load(bool *fromFlash = nullptr);

// Writes the settings if they differ from what is stored. Returns true if it wrote.
bool save(const Settings &s);

// Erases the stored copy, so the next boot uses config.h again.
void clear();

}  // namespace settings
