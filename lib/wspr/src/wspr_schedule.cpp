#include "wspr_schedule.h"

namespace wspr {

bool SlotSchedule::setEveryN(int n) {
  if (n < 1 || n > MAX_EVERY_N) return false;
  everyN_ = n;
  return true;
}

bool SlotSchedule::due(int64_t slot) const {
  return nextDue(slot) == slot;
}

int64_t SlotSchedule::nextDue(int64_t slot) const {
  if (requested_ || (enabled_ && lastSlot_ < 0)) return slot;
  if (!enabled_) return -1;
  int64_t next = lastSlot_ + everyN_;
  return next > slot ? next : slot;
}

void SlotSchedule::markTransmitted(int64_t slot) {
  lastSlot_ = slot;
  requested_ = false;
}

}  // namespace wspr
