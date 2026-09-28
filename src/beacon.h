// WSPR beacon: picks the transmit slots (every N-th 2-minute slot after the last
// transmission) and keys the message symbols onto the radio with timing anchored
// to the UTC slot start.
#pragma once

#include <stdint.h>

#include "radio.h"
#include "time_sync.h"
#include "wspr.h"

class Beacon {
 public:
  // Which message part goes out on each transmission (only matters with a 6-char locator).
  enum class MsgMode : uint8_t { Alternate = 0, Type1Only = 1, Type3Only = 2 };

  // One entry of the transmission log (kept in RAM, newest first).
  struct TxRecord {
    enum class Status : uint8_t { OnAir, Done, Aborted };
    int64_t startUtc;  // UTC seconds of symbol 0
    uint64_t freqHz;
    uint8_t part;      // message part (0 = Type 1, 1 = Type 3)
    Status status;
  };
  static constexpr int HISTORY_SIZE = 20;

  Beacon(Radio &radio, const wspr::Message &message, const TimeSync &time);

  void setup(uint64_t centerHz, int everyNSlots, int randomOffsetHz);

  // Call often from loop(): starts transmissions on schedule and steps through the symbols.
  void service();

  // Scheduled transmissions on/off. Turning it off aborts a transmission in progress.
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

  void setEveryNSlots(int n) { everyNSlots_ = n; }
  int everyNSlots() const { return everyNSlots_; }

  // Transmit in the next slot, regardless of the N-slot schedule.
  void requestNextSlot() { nextRequested_ = true; }
  bool nextRequested() const { return nextRequested_; }

  // Clears a pending request and aborts a transmission. Returns true if one was aborted.
  bool cancel();

  // No new transmissions start while paused (e.g. the test carrier is on).
  void setPaused(bool paused) { paused_ = paused; }

  void setCenterHz(uint64_t hz) { centerHz_ = hz; }
  uint64_t centerHz() const { return centerHz_; }

  bool transmitting() const { return transmitting_; }
  int currentSymbol() const { return symbol_; }
  int currentPart() const { return part_; }  // which message part (Type 1 / Type 3) is on air

  // Returns false if the mode needs a Type 3 part and the locator has only 4 chars.
  bool setMsgMode(MsgMode mode);
  MsgMode msgMode() const { return msgMode_; }
  int nextPart() const;  // message part the next transmission will send
  uint64_t txFreqHz() const { return txFreqHz_; }

  // UTC seconds of the next transmission, or -1 if none is scheduled.
  int64_t nextTxStart() const;

  // UTC seconds of the last transmission start, or -1 if none yet.
  int64_t lastTxStart() const;

  // Transmission log: history(0) is the most recent. historyCount() <= HISTORY_SIZE.
  int historyCount() const { return historyCount_; }
  const TxRecord &history(int i) const {
    return history_[(historyHead_ + HISTORY_SIZE - 1 - i) % HISTORY_SIZE];
  }

 private:
  bool isTxSlot(int64_t slot) const;
  void checkSchedule();
  void start(int64_t slotStartUtcUs);
  void stepSymbols();
  void finish(const char *why, bool completed);

  Radio &radio_;
  const wspr::Message &message_;
  const TimeSync &time_;

  uint64_t centerHz_ = 0;
  int everyNSlots_ = 5;
  int randomOffsetHz_ = 0;
  bool enabled_ = true;
  bool paused_ = false;
  bool nextRequested_ = false;
  int64_t lastHandledSlot_ = -1;
  int64_t lastTxSlot_ = -1;      // slot of the last transmission (aborted ones count)

  bool transmitting_ = false;
  int64_t startUs_ = 0;  // esp_timer time of symbol 0
  int symbol_ = -1;
  int part_ = 0;
  uint32_t txCount_ = 0;  // alternates the message parts
  MsgMode msgMode_ = MsgMode::Alternate;

  TxRecord history_[HISTORY_SIZE] = {};
  int historyHead_ = 0;   // next slot to write
  int historyCount_ = 0;
  uint64_t txFreqHz_ = 0;
};
