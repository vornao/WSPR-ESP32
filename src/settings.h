// Runtime settings kept in flash (NVS), so changes made from the console or the web page
// survive a restart. Defaults come from config.h; "restore defaults" clears the flash copy.
#pragma once

#include <stdint.h>

struct Settings {
  uint64_t centerHz;
  int32_t correctionPpb;
  uint8_t driveMa;
  uint8_t everyNSlots;
  uint8_t msgMode;  // Beacon::MsgMode
  bool beaconOn;

  bool operator==(const Settings &o) const {
    return centerHz == o.centerHz && correctionPpb == o.correctionPpb && driveMa == o.driveMa &&
           everyNSlots == o.everyNSlots && msgMode == o.msgMode && beaconOn == o.beaconOn;
  }
  bool operator!=(const Settings &o) const { return !(*this == o); }
};

namespace settings {

// The values from config.h.
Settings defaults();

// Stored values, with defaults for anything missing. `fromFlash` tells whether any were stored.
Settings load(bool *fromFlash = nullptr);

// Writes the settings if they differ from what is stored. Returns true if it wrote.
bool save(const Settings &s);

// Erases the stored copy, so the next boot uses config.h again.
void clear();

}  // namespace settings
