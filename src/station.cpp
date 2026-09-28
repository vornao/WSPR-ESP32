#include "station.h"

#include <Arduino.h>
#include <si5351.h>

Station::Station(Radio &radio, Beacon &beacon) : radio_(radio), beacon_(beacon) {}

Station::Result Station::checkRadio(bool refuseWhileTransmitting) const {
  if (!radio_.ok()) return Result::NoRadio;
  if (refuseWhileTransmitting && beacon_.transmitting()) return Result::Busy;
  return Result::Ok;
}

Station::Result Station::persist(Result r) {
  if (r == Result::Ok && settings::save(current())) Serial.println("Settings saved.");
  return r;
}

Settings Station::current() const {
  Settings s;
  s.centerHz = beacon_.centerHz();
  s.correctionPpb = radio_.correctionPpb();
  s.driveMa = (uint8_t)radio_.driveMa();
  s.everyNSlots = (uint8_t)beacon_.everyNSlots();
  s.msgMode = (uint8_t)beacon_.msgMode();
  s.beaconOn = beacon_.enabled();
  return s;
}

void Station::apply(const Settings &s) {
  beacon_.setCenterHz(s.centerHz);
  if (s.everyNSlots >= 1 && s.everyNSlots <= 30) beacon_.setEveryNSlots(s.everyNSlots);
  if (s.msgMode > 2 || !beacon_.setMsgMode((Beacon::MsgMode)s.msgMode))
    beacon_.setMsgMode(Beacon::MsgMode::Type1Only);
  beacon_.setEnabled(s.beaconOn);
  if (!radio_.ok()) return;
  radio_.setCorrectionPpb(s.correctionPpb);
  radio_.setDriveMa(s.driveMa);
  radio_.setFrequencyCentiHz(s.centerHz * SI5351_FREQ_MULT);
}

Station::Result Station::restoreDefaults() {
  Result r = checkRadio(true);
  if (r != Result::Ok) return r;
  if (carrierOn_) setCarrier(false);
  apply(settings::defaults());
  settings::clear();
  return Result::Ok;
}

Station::Result Station::setCenterHz(int64_t hz) {
  Result r = checkRadio(true);
  if (r != Result::Ok) return r;
  if (hz < 8000 || hz > 160000000) return Result::Invalid;
  beacon_.setCenterHz((uint64_t)hz);
  radio_.setFrequencyCentiHz((uint64_t)hz * SI5351_FREQ_MULT);
  return persist(Result::Ok);
}

Station::Result Station::setCorrectionPpb(int64_t ppb) {
  Result r = checkRadio(true);
  if (r != Result::Ok) return r;
  if (ppb < -1000000 || ppb > 1000000) return Result::Invalid;
  radio_.setCorrectionPpb((int32_t)ppb);
  return persist(Result::Ok);
}

Station::Result Station::setDriveMa(int64_t ma) {
  Result r = checkRadio(false);
  if (r != Result::Ok) return r;
  return persist(radio_.setDriveMa((int)ma) ? Result::Ok : Result::Invalid);
}

Station::Result Station::setCarrier(bool on) {
  Result r = checkRadio(true);
  if (r != Result::Ok) return r;
  carrierOn_ = on;
  if (on) radio_.setFrequencyCentiHz(beacon_.centerHz() * SI5351_FREQ_MULT);
  radio_.setOutput(on);
  beacon_.setPaused(on);
  return Result::Ok;
}

Station::Result Station::setBeaconEnabled(bool on) {
  Result r = checkRadio(false);
  if (r != Result::Ok) return r;
  beacon_.setEnabled(on);
  return persist(Result::Ok);
}

Station::Result Station::setEveryNSlots(int64_t n) {
  Result r = checkRadio(false);
  if (r != Result::Ok) return r;
  if (n < 1 || n > 30) return Result::Invalid;
  beacon_.setEveryNSlots((int)n);
  return persist(Result::Ok);
}

Station::Result Station::setMsgMode(int64_t mode) {
  if (mode < 0 || mode > 2) return Result::Invalid;
  return persist(beacon_.setMsgMode((Beacon::MsgMode)mode) ? Result::Ok : Result::Invalid);
}

Station::Result Station::requestNextSlot() {
  Result r = checkRadio(false);
  if (r != Result::Ok) return r;
  beacon_.requestNextSlot();
  return Result::Ok;
}

bool Station::cancel() {
  return beacon_.cancel();
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
