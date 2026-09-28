// Control actions shared by the serial console and the web interface.
// Each one validates its input and refuses changes that would disturb a transmission.
#pragma once

#include <stdint.h>

#include "beacon.h"
#include "radio.h"
#include "settings.h"

class Station {
 public:
  enum class Result { Ok, Invalid, Busy, NoRadio };

  Station(Radio &radio, Beacon &beacon);

  Result setCenterHz(int64_t hz);          // 8 kHz .. 160 MHz
  Result setCorrectionPpb(int64_t ppb);    // -1000000 .. 1000000, re-applied immediately
  Result setDriveMa(int64_t ma);           // 2, 4, 6 or 8
  Result setCarrier(bool on);              // steady test carrier; pauses the beacon
  Result setBeaconEnabled(bool on);
  Result setEveryNSlots(int64_t n);        // 1 .. 30
  Result setMsgMode(int64_t mode);         // 0 alternate T1/T3, 1 Type 1 only, 2 Type 3 only; next TX
  Result requestNextSlot();

  // Applies stored settings at boot (does not write flash).
  void apply(const Settings &s);

  // Back to the config.h values, and erases the stored copy.
  Result restoreDefaults();

  // The current values of everything that is persisted.
  Settings current() const;

  // Clears a pending request and aborts a transmission. Returns true if one was aborted.
  bool cancel();

  bool carrierOn() const { return carrierOn_; }

  static const char *describe(Result r);

 private:
  Result checkRadio(bool refuseWhileTransmitting) const;
  Result persist(Result r);  // saves current() to flash when r is Ok

  Radio &radio_;
  Beacon &beacon_;
  bool carrierOn_ = false;
};
