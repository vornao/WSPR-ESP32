#include "station.h"

#include <Arduino.h>

using wspr::Beacon;

Station::Station(Radio &radio, Beacon &beacon, uint32_t toneDwellMs)
    : radio_(radio), beacon_(beacon), toneDwellMs_(toneDwellMs) {}

template <typename Fn>
Station::Result Station::whenIdle(Fn change) {
  if (!radio_.ready()) return Result::NoRadio;
  return beacon_.whenIdle(change) ? Result::Ok : Result::Busy;
}

Station::Result Station::persist() {
  settings::save(current());
  return Result::Ok;
}

Settings Station::current() const {
  Beacon::State b = beacon_.state();
  Settings s;
  s.centerHz = b.centerHz;
  s.correctionPpb = radio_.correctionPpb();
  s.driveMa = (uint8_t)radio_.driveMa();
  s.everyNSlots = (uint8_t)b.everyNSlots;
  s.msgMode = (uint8_t)b.msgMode;
  s.beaconOn = b.enabled;
  return s;
}

void Station::apply(const Settings &s) {
  beacon_.setCenterHz(s.centerHz);
  beacon_.setEveryNSlots(s.everyNSlots);
  if (!beacon_.setMsgMode((Beacon::MsgMode)s.msgMode)) beacon_.setMsgMode(Beacon::MsgMode::Type1Only);
  beacon_.setEnabled(s.beaconOn);
  radio_.setCorrectionPpb(s.correctionPpb);
  radio_.setDriveMa(s.driveMa);
  radio_.setFrequencyCentiHz(s.centerHz * wspr::CENTI_HZ);
}

Station::Result Station::restoreDefaults() {
  Result r = whenIdle([this] {
    if (testMode_ != TestMode::Off) startTest(TestMode::Off);
    apply(settings::defaults());
  });
  if (r == Result::Ok) settings::clear();
  return r;
}

// ---------- radio ----------

Station::Result Station::setCenterHz(int64_t hz) {
  if (!limits::centerHz(hz)) return Result::Invalid;
  Result r = whenIdle([&] {
    beacon_.setCenterHz((uint64_t)hz);
    if (testMode_ != TestMode::Tones) radio_.setFrequencyCentiHz((uint64_t)hz * wspr::CENTI_HZ);
  });
  return r == Result::Ok ? persist() : r;
}

// Re-programs the frequency on air, so not during a transmission.
Station::Result Station::setCorrectionPpb(int64_t ppb) {
  if (!limits::correctionPpb(ppb)) return Result::Invalid;
  Result r = whenIdle([&] { radio_.setCorrectionPpb((int32_t)ppb); });
  return r == Result::Ok ? persist() : r;
}

// Only changes the amplitude, so it is allowed during a transmission.
Station::Result Station::setDriveMa(int64_t ma) {
  if (!radio_.ready()) return Result::NoRadio;
  if (!limits::driveMa(ma)) return Result::Invalid;
  radio_.setDriveMa((int)ma);
  return persist();
}

// ---------- beacon ----------

Station::Result Station::setBeaconEnabled(bool on) {
  if (!radio_.ready()) return Result::NoRadio;
  beacon_.setEnabled(on);
  return persist();
}

Station::Result Station::setEveryNSlots(int64_t n) {
  if (!limits::everyNSlots(n)) return Result::Invalid;
  beacon_.setEveryNSlots((int)n);
  return persist();
}

Station::Result Station::setMsgMode(int64_t mode) {
  if (!limits::msgMode(mode) || !beacon_.setMsgMode((Beacon::MsgMode)mode)) return Result::Invalid;
  return persist();
}

Station::Result Station::requestNextSlot() {
  if (!radio_.ready()) return Result::NoRadio;
  beacon_.requestNextSlot();
  return Result::Ok;
}

bool Station::cancel() {
  return beacon_.cancel();
}

// ---------- test output ----------

Station::Result Station::setTestMode(TestMode mode) {
  if (mode == testMode_) return Result::Ok;
  if (suspended_) return Result::Busy;
  return whenIdle([&] { startTest(mode); });
}

void Station::startTest(TestMode mode) {
  testMode_ = mode;
  tone_ = 0;
  toneSinceMs_ = millis();
  uint64_t centerCentiHz = beacon_.state().centerHz * wspr::CENTI_HZ;
  radio_.setFrequencyCentiHz(centerCentiHz);  // tone 0 is the centre frequency
  radio_.setOutput(mode != TestMode::Off);
  updatePause();
}

void Station::service() {
  if (testMode_ != TestMode::Tones || millis() - toneSinceMs_ < toneDwellMs_) return;
  toneSinceMs_ = millis();
  tone_ = (tone_ + 1) % 4;
  radio_.setFrequencyCentiHz(wspr::toneCentiHz(beacon_.state().centerHz, (uint8_t)tone_));
}

// ---------- firmware update ----------

void Station::suspend() {
  suspended_ = true;
  beacon_.cancel();
  if (testMode_ != TestMode::Off) startTest(TestMode::Off);
  radio_.setOutput(false);
  updatePause();
}

void Station::resume() {
  suspended_ = false;
  updatePause();
}

void Station::updatePause() {
  beacon_.setPaused(suspended_ || testMode_ != TestMode::Off);
}

const char *Station::describe(Result r) {
  switch (r) {
    case Result::Ok: return "ok";
    case Result::Invalid: return "invalid value";
    case Result::Busy: return "transmitting, abort first";
    case Result::NoRadio: return "Si5351 not responding";
  }
  return "?";
}
