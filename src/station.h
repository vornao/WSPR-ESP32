// Control actions shared by the serial console and the web interface.
// Each one validates its input, refuses changes that would disturb a transmission, and
// saves what it changed to flash.
#pragma once

#include <stdint.h>

#include <wspr_beacon.h>

#include "radio.h"
#include "settings.h"

class Station {
 public:
  enum class Result { Ok, Invalid, Busy, NoRadio };

  // Output for testing, outside the beacon schedule. Both pause the beacon.
  enum class TestMode : uint8_t {
    Off,
    Carrier,  // steady carrier on the centre frequency
    Tones,    // the four WSPR tones in turn, to check the fine frequency steps
  };

  Station(Radio &radio, wspr::Beacon &beacon, uint32_t toneDwellMs);

  // Applies stored settings at boot (does not write flash).
  void apply(const Settings &s);

  // Call often from loop(): steps the tone sweep.
  void service();

  Result setCenterHz(int64_t hz);
  Result setCorrectionPpb(int64_t ppb);
  Result setDriveMa(int64_t ma);
  Result setBeaconEnabled(bool on);
  Result setEveryNSlots(int64_t n);
  Result setMsgMode(int64_t mode);  // 0 alternate T1/T3, 1 Type 1 only, 2 Type 3 only
  Result setTestMode(TestMode mode);
  Result requestNextSlot();

  // Clears a pending request and aborts a transmission. Returns true if one was aborted.
  bool cancel();

  // Back to the config.h values, and erases the stored copy.
  Result restoreDefaults();

  // Off the air and on hold until resume(), e.g. while a firmware update is received.
  void suspend();
  void resume();

  TestMode testMode() const { return testMode_; }
  int testTone() const { return testMode_ == TestMode::Tones ? tone_ : -1; }

  // The current values of everything that is persisted.
  Settings current() const;

  static const char *describe(Result r);

 private:
  // Runs `change` unless a transmission is on air (the beacon can't start one meanwhile).
  template <typename Fn>
  Result whenIdle(Fn change);
  Result persist();  // saves current() to flash
  void startTest(TestMode mode);
  void updatePause();

  Radio &radio_;
  wspr::Beacon &beacon_;
  const uint32_t toneDwellMs_;

  TestMode testMode_ = TestMode::Off;
  int tone_ = 0;
  uint32_t toneSinceMs_ = 0;
  bool suspended_ = false;
};
