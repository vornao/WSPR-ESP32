// ESP32 only (FreeRTOS task, esp_timer); left out of the host unit-test build.
#ifdef ESP_PLATFORM

#include "wspr_beacon.h"

#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace wspr {

namespace {

constexpr uint32_t IDLE_POLL_MS = 10;  // how late a transmission can start; the lateness is compensated
constexpr uint32_t TASK_STACK_BYTES = 4096;

// Uniform in [-range, range].
int randomOffset(int range) {
  return range > 0 ? (int)(esp_random() % (uint32_t)(2 * range + 1)) - range : 0;
}

}  // namespace

Beacon::Beacon(Transmitter &tx, const Message &message, const Clock &clock)
    : tx_(tx), message_(message), clock_(clock) {}

bool Beacon::begin(unsigned priority) {
  return xTaskCreate(taskMain, "wspr_beacon", TASK_STACK_BYTES, this, priority, nullptr) == pdPASS;
}

void Beacon::taskMain(void *self) {
  Beacon &beacon = *static_cast<Beacon *>(self);
  for (;;) {
    TickType_t ticks = pdMS_TO_TICKS(beacon.tick());
    vTaskDelay(ticks > 0 ? ticks : 1);  // always block, or loop() would never run
  }
}

// ---------- settings ----------

void Beacon::setEnabled(bool on) {
  Guard g(mutex_);
  schedule_.setEnabled(on);
  if (!on && transmitting_) finish(TxRecord::Status::Aborted);
}

bool Beacon::setEveryNSlots(int n) {
  Guard g(mutex_);
  return schedule_.setEveryN(n);
}

void Beacon::setCenterHz(uint64_t hz) {
  Guard g(mutex_);
  centerHz_ = hz;
}

bool Beacon::setRandomOffsetHz(int hz) {
  if (hz < 0 || hz > MAX_RANDOM_OFFSET_HZ) return false;
  Guard g(mutex_);
  randomOffsetHz_ = hz;
  return true;
}

bool Beacon::setMsgMode(MsgMode mode) {
  if (mode != MsgMode::Type1Only && message_.parts() < 2) return false;
  Guard g(mutex_);
  msgMode_ = mode;
  return true;
}

void Beacon::setPaused(bool paused) {
  Guard g(mutex_);
  paused_ = paused;
}

void Beacon::requestNextSlot() {
  Guard g(mutex_);
  schedule_.request();
}

bool Beacon::cancel() {
  Guard g(mutex_);
  schedule_.clearRequest();
  if (!transmitting_) return false;
  finish(TxRecord::Status::Aborted);
  return true;
}

// ---------- queries ----------

Beacon::State Beacon::state() const {
  Guard g(mutex_);
  State s;
  s.enabled = schedule_.enabled();
  s.paused = paused_;
  s.requested = schedule_.requested();
  s.transmitting = transmitting_;
  s.everyNSlots = schedule_.everyN();
  s.randomOffsetHz = randomOffsetHz_;
  s.msgMode = msgMode_;
  s.centerHz = centerHz_;
  s.txFreqHz = txFreqHz_;
  s.symbol = symbol_;
  s.part = part_;
  s.nextPart = nextPart();
  s.nextTxUtc = nextTxUtc();
  s.lastTxUtc = schedule_.lastSlot() < 0 ? -1 : slotStartS(schedule_.lastSlot());
  s.changes = changes_;
  return s;
}

bool Beacon::transmitting() const {
  Guard g(mutex_);
  return transmitting_;
}

int Beacon::history(TxRecord *out, int max) const {
  Guard g(mutex_);
  int n = historyCount_ < max ? historyCount_ : max;
  for (int i = 0; i < n; i++) out[i] = history_[(historyHead_ + HISTORY_SIZE - 1 - i) % HISTORY_SIZE];
  return n;
}

uint32_t Beacon::changes() const {
  Guard g(mutex_);
  return changes_;
}

int Beacon::nextPart() const {
  if (message_.parts() < 2) return Message::TYPE1;
  switch (msgMode_) {
    case MsgMode::Type1Only: return Message::TYPE1;
    case MsgMode::Type3Only: return Message::TYPE3;
    default: return (int)(alternateCount_ % 2);
  }
}

int64_t Beacon::nextTxUtc() const {
  if (!clock_.synced()) return -1;
  int64_t now = clock_.utcUs() / 1000000;
  int64_t slot = now / SLOT_S;
  if (now >= slotStartS(slot)) slot++;  // this slot's start second has passed
  int64_t due = schedule_.nextDue(slot);
  return due < 0 ? -1 : slotStartS(due);
}

// ---------- the beacon task ----------

uint32_t Beacon::tick() {
  Guard g(mutex_);
  if (!transmitting_) maybeStart();
  if (!transmitting_) return IDLE_POLL_MS;

  int sym = symbolAt(esp_timer_get_time() - startUs_);
  if (sym >= SYMBOL_COUNT) {
    finish(TxRecord::Status::Done);
    return IDLE_POLL_MS;
  }
  if (sym != symbol_) {
    bool first = symbol_ < 0;
    symbol_ = sym;
    tx_.setFrequencyCentiHz(toneCentiHz(txFreqHz_, message_.symbol(part_, sym)));
    if (first) tx_.setOutput(true);  // key only once the first tone is programmed
  }
  // Sleep until the next symbol is due (rounded down; waking early is harmless).
  int64_t waitUs = symbolStartUs(sym + 1) - (esp_timer_get_time() - startUs_);
  return waitUs > 1000 ? (uint32_t)(waitUs / 1000) : 1;
}

// Starts a transmission if we are in the start second of a slot that is due.
void Beacon::maybeStart() {
  if (paused_ || !tx_.ready() || !message_.valid() || !clock_.synced()) return;
  int64_t nowUs = clock_.utcUs();
  int64_t now = nowUs / 1000000;
  int64_t slot = now / SLOT_S;
  if (now != slotStartS(slot) || slot == lastCheckedSlot_) return;
  lastCheckedSlot_ = slot;
  if (schedule_.due(slot)) start(slot, nowUs);
}

void Beacon::start(int64_t slot, int64_t nowUtcUs) {
  // Anchor the symbol timing to the monotonic timer, aligned to the UTC slot start, so an
  // NTP adjustment during the transmission can't disturb it.
  int64_t lateUs = nowUtcUs - slotStartS(slot) * 1000000;
  startUs_ = esp_timer_get_time() - lateUs;
  txFreqHz_ = centerHz_ + randomOffset(randomOffsetHz_);
  part_ = nextPart();
  if (msgMode_ == MsgMode::Alternate) alternateCount_++;
  symbol_ = -1;
  transmitting_ = true;
  schedule_.markTransmitted(slot);

  history_[historyHead_] = TxRecord{slotStartS(slot), txFreqHz_, (uint8_t)part_, TxRecord::Status::OnAir};
  historyHead_ = (historyHead_ + 1) % HISTORY_SIZE;
  if (historyCount_ < HISTORY_SIZE) historyCount_++;
  changes_++;
}

void Beacon::finish(TxRecord::Status status) {
  tx_.setOutput(false);
  transmitting_ = false;
  symbol_ = -1;
  history_[(historyHead_ + HISTORY_SIZE - 1) % HISTORY_SIZE].status = status;
  changes_++;
}

}  // namespace wspr

#endif  // ESP_PLATFORM
