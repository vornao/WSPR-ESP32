// Which 2-minute slots to transmit in: every N-th slot after the previous transmission,
// the first slot once the beacon is enabled, and the next slot when one is requested.
// Pure logic with no clock of its own, so it is unit-tested on the host.
#pragma once

#include <stdint.h>

namespace wspr {

class SlotSchedule {
 public:
  static constexpr int MAX_EVERY_N = 30;  // one transmission per hour

  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }

  // 1 .. MAX_EVERY_N; returns false (unchanged) otherwise.
  bool setEveryN(int n);
  int everyN() const { return everyN_; }

  // One transmission in the next slot, whether or not the schedule is enabled.
  void request() { requested_ = true; }
  void clearRequest() { requested_ = false; }
  bool requested() const { return requested_; }

  // Whether a transmission should start in `slot`.
  bool due(int64_t slot) const;

  // The first slot >= `slot` that is due, or -1 if nothing is scheduled.
  int64_t nextDue(int64_t slot) const;

  // Records a transmission in `slot` (aborted ones count) and consumes any request.
  void markTransmitted(int64_t slot);
  int64_t lastSlot() const { return lastSlot_; }  // -1 if none yet

 private:
  bool enabled_ = true;
  bool requested_ = false;
  int everyN_ = 1;
  int64_t lastSlot_ = -1;
};

}  // namespace wspr
