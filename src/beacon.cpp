#include "beacon.h"

#include <Arduino.h>
#include <esp_timer.h>

#include "config.h"

Beacon::Beacon(Radio &radio, const wspr::Message &message, const TimeSync &time)
    : radio_(radio), message_(message), time_(time) {}

void Beacon::setup(uint64_t centerHz, int everyNSlots, int randomOffsetHz) {
  centerHz_ = centerHz;
  everyNSlots_ = everyNSlots;
  randomOffsetHz_ = randomOffsetHz;
}

void Beacon::service() {
  if (transmitting_) stepSymbols();
  else checkSchedule();
}

void Beacon::setEnabled(bool on) {
  enabled_ = on;
  if (!on && transmitting_) finish("aborted (beacon off)", false);
}

bool Beacon::cancel() {
  nextRequested_ = false;
  if (!transmitting_) return false;
  finish("aborted", false);
  return true;
}

// ---------- message parts ----------

bool Beacon::setMsgMode(MsgMode mode) {
  if (mode != MsgMode::Type1Only && message_.parts() < 2) return false;
  msgMode_ = mode;
  return true;
}

int Beacon::nextPart() const {
  if (message_.parts() < 2) return 0;
  switch (msgMode_) {
    case MsgMode::Type1Only: return 0;
    case MsgMode::Type3Only: return 1;
    default: return (int)(txCount_ % 2);
  }
}

// ---------- scheduling ----------

// The next transmission is N slots after the last one, or the first slot after
// the beacon starts / a manual request.
bool Beacon::isTxSlot(int64_t slot) const {
  return nextRequested_ || lastTxSlot_ < 0 || slot >= lastTxSlot_ + everyNSlots_;
}

int64_t Beacon::nextTxStart() const {
  if (!time_.synced() || !(enabled_ || nextRequested_)) return -1;
  int64_t now = TimeSync::nowUtcUs() / 1000000LL;
  int64_t slot = now / wspr::SLOT_S;
  if (now % wspr::SLOT_S >= wspr::START_OFFSET_S) slot++;
  if (!isTxSlot(slot)) slot = lastTxSlot_ + everyNSlots_;
  return slot * wspr::SLOT_S + wspr::START_OFFSET_S;
}

int64_t Beacon::lastTxStart() const {
  return lastTxSlot_ < 0 ? -1 : lastTxSlot_ * wspr::SLOT_S + wspr::START_OFFSET_S;
}

// Starts a transmission when we reach the start second of a slot we should transmit in.
void Beacon::checkSchedule() {
  if (!radio_.ok() || !time_.synced() || paused_) return;
  if (!enabled_ && !nextRequested_) return;

  int64_t now = TimeSync::nowUtcUs() / 1000000LL;
  int64_t slot = now / wspr::SLOT_S;
  if (now % wspr::SLOT_S != wspr::START_OFFSET_S || slot == lastHandledSlot_) return;
  lastHandledSlot_ = slot;
  if (!isTxSlot(slot)) return;

  if (!message_.valid()) {
    Serial.println("Skipping TX slot: WSPR message invalid (check LOCATOR in config.h).");
    return;
  }
  lastTxSlot_ = slot;
  start((slot * wspr::SLOT_S + wspr::START_OFFSET_S) * 1000000LL);
}

// ---------- transmission ----------

void Beacon::start(int64_t slotStartUtcUs) {
  // Anchor symbol timing to the monotonic timer, aligned to the UTC slot start,
  // so an NTP adjustment mid-transmission can't disturb it.
  int64_t lateUs = TimeSync::nowUtcUs() - slotStartUtcUs;
  startUs_ = esp_timer_get_time() - lateUs;
  txFreqHz_ = centerHz_ + random(-randomOffsetHz_, randomOffsetHz_ + 1);
  symbol_ = -1;
  part_ = nextPart();
  if (msgMode_ == MsgMode::Alternate) txCount_++;
  transmitting_ = true;
  nextRequested_ = false;

  TxRecord &rec = history_[historyHead_];
  rec.startUtc = slotStartUtcUs / 1000000LL;
  rec.freqHz = txFreqHz_;
  rec.part = (uint8_t)part_;
  rec.status = TxRecord::Status::OnAir;
  historyHead_ = (historyHead_ + 1) % HISTORY_SIZE;
  if (historyCount_ < HISTORY_SIZE) historyCount_++;

  Serial.print("TX start ");
  printUtc((time_t)(slotStartUtcUs / 1000000LL));
  Serial.printf(" | %llu Hz | %s dBm | %lld ms late\n", txFreqHz_, message_.partName(part_),
                lateUs / 1000);

  stepSymbols();  // program symbol 0 before keying
  radio_.setOutput(true);
}

void Beacon::stepSymbols() {
  int sym = wspr::symbolAt(esp_timer_get_time() - startUs_);
  if (sym >= wspr::SYMBOL_COUNT) {
    finish("done", true);
    return;
  }
  if (sym != symbol_) {
    symbol_ = sym;
    radio_.setFrequencyCentiHz(wspr::toneCentiHz(txFreqHz_, message_.symbol(part_, sym)));
  }
}

void Beacon::finish(const char *why, bool completed) {
  radio_.setOutput(false);
  transmitting_ = false;
  if (historyCount_ > 0) {
    TxRecord &rec = history_[(historyHead_ + HISTORY_SIZE - 1) % HISTORY_SIZE];
    if (rec.status == TxRecord::Status::OnAir)
      rec.status = completed ? TxRecord::Status::Done : TxRecord::Status::Aborted;
  }
  Serial.printf("TX %s\n", why);

  int64_t next = nextTxStart();
  if (next > 0) {
    Serial.print("Next TX ");
    printUtc((time_t)next);
    Serial.println();
  }
}
