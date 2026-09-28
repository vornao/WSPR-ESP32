// WSPR beacon engine (ESP32). A dedicated FreeRTOS task starts transmissions on schedule
// and keys the message symbols onto a Transmitter, with the timing anchored to the UTC
// slot start. Because it does not run in loop(), a slow web client, a flash write or a
// blocked serial port cannot stall a transmission.
//
// Every public method is thread-safe.
#pragma once

#include <stdint.h>

#include <mutex>

#include "wspr_message.h"
#include "wspr_protocol.h"
#include "wspr_schedule.h"

namespace wspr {

// The RF output the beacon keys. Called from the beacon task: implementations must be
// safe to use from more than one task.
class Transmitter {
 public:
  virtual ~Transmitter() = default;
  virtual bool ready() const = 0;
  virtual void setFrequencyCentiHz(uint64_t centiHz) = 0;
  virtual void setOutput(bool on) = 0;
};

// UTC time source.
class Clock {
 public:
  virtual ~Clock() = default;
  virtual bool synced() const = 0;  // true while the time is good to well under a second
  virtual int64_t utcUs() const = 0;
};

class Beacon {
 public:
  // Which message part goes out on each transmission (only matters with a 6-char locator).
  enum class MsgMode : uint8_t { Alternate = 0, Type1Only = 1, Type3Only = 2 };

  // One entry of the transmission log, kept in RAM.
  struct TxRecord {
    enum class Status : uint8_t { OnAir, Done, Aborted };
    int64_t startUtc;  // UTC second of symbol 0
    uint64_t freqHz;   // tone 0
    uint8_t part;      // Message::TYPE1 or Message::TYPE3
    Status status;
  };
  static constexpr int HISTORY_SIZE = 20;

  // A consistent snapshot of the beacon.
  struct State {
    bool enabled;       // scheduled transmissions on
    bool paused;        // no new transmissions (test carrier, firmware update)
    bool requested;     // one transmission requested for the next slot
    bool transmitting;
    int everyNSlots;
    int randomOffsetHz;
    MsgMode msgMode;
    uint64_t centerHz;
    uint64_t txFreqHz;  // while transmitting
    int symbol;         // 0-based symbol on air, while transmitting
    int part;           // message part on air, while transmitting
    int nextPart;       // message part the next transmission will send
    int64_t nextTxUtc;  // UTC second of the next transmission, -1 if none is scheduled
    int64_t lastTxUtc;  // UTC second of the last transmission, -1 if none yet
    uint32_t changes;   // see changes()
  };

  // `message` must stay unchanged once begin() has been called.
  Beacon(Transmitter &tx, const Message &message, const Clock &clock);
  Beacon(const Beacon &) = delete;
  Beacon &operator=(const Beacon &) = delete;

  // Starts the beacon task. Its priority must be above loop()'s (1) so it can pre-empt it.
  bool begin(unsigned priority = 5);

  // Scheduled transmissions on/off. Turning it off aborts a transmission in progress.
  void setEnabled(bool on);

  // Every n-th 2-minute slot, 1 .. SlotSchedule::MAX_EVERY_N. Returns false if out of range.
  bool setEveryNSlots(int n);

  // Each transmission goes out on centerHz ± a random offset of up to `randomOffsetHz`
  // (0 .. MAX_RANDOM_OFFSET_HZ). Takes effect from the next transmission.
  void setCenterHz(uint64_t hz);
  bool setRandomOffsetHz(int hz);

  // Returns false if the mode needs a Type 3 part and the message has none.
  bool setMsgMode(MsgMode mode);

  // Holds off new transmissions (does not abort one in progress).
  void setPaused(bool paused);

  // Transmit in the next slot, regardless of the schedule.
  void requestNextSlot();

  // Clears a pending request and aborts a transmission. Returns true if one was aborted.
  bool cancel();

  State state() const;
  bool transmitting() const;

  // Copies up to `max` log entries into `out`, newest first. Returns how many.
  int history(TxRecord *out, int max) const;

  // Incremented whenever a transmission starts or ends: poll it to follow the log.
  uint32_t changes() const;

  // Runs `change` with the beacon held idle: no transmission is on air, and none can start
  // until it returns. Returns false, without running it, if one is on air. `change` may
  // call Beacon methods.
  template <typename Fn>
  bool whenIdle(Fn change) {
    Guard g(mutex_);
    if (transmitting_) return false;
    change();
    return true;
  }

 private:
  using Guard = std::lock_guard<std::recursive_mutex>;

  static void taskMain(void *self);
  uint32_t tick();  // one pass of the task; returns how long it may sleep, in ms
  void maybeStart();
  void start(int64_t slot, int64_t nowUtcUs);
  void finish(TxRecord::Status status);
  int nextPart() const;
  int64_t nextTxUtc() const;

  Transmitter &tx_;
  const Message &message_;
  const Clock &clock_;
  mutable std::recursive_mutex mutex_;

  SlotSchedule schedule_;
  uint64_t centerHz_ = 0;
  int randomOffsetHz_ = 0;
  MsgMode msgMode_ = MsgMode::Alternate;
  bool paused_ = false;
  int64_t lastCheckedSlot_ = -1;

  bool transmitting_ = false;
  int64_t startUs_ = 0;  // esp_timer time of symbol 0
  uint64_t txFreqHz_ = 0;
  int symbol_ = -1;
  int part_ = 0;
  uint32_t alternateCount_ = 0;

  TxRecord history_[HISTORY_SIZE] = {};
  int historyHead_ = 0;  // next entry to write
  int historyCount_ = 0;
  uint32_t changes_ = 0;
};

}  // namespace wspr
