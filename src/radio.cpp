#include "radio.h"

#include <Wire.h>

namespace {

uint8_t loadCapacitance(int pf) {
  switch (pf) {
    case 6: return SI5351_CRYSTAL_LOAD_6PF;
    case 10: return SI5351_CRYSTAL_LOAD_10PF;
    default: return SI5351_CRYSTAL_LOAD_8PF;
  }
}

si5351_drive driveEnum(int ma) {
  switch (ma) {
    case 2: return SI5351_DRIVE_2MA;
    case 4: return SI5351_DRIVE_4MA;
    case 6: return SI5351_DRIVE_6MA;
    default: return SI5351_DRIVE_8MA;
  }
}

}  // namespace

bool Radio::begin(const Config &config, int32_t correctionPpb, int driveMa, uint64_t initialCentiHz) {
  Guard g(mutex_);
  Wire.begin(config.sdaPin, config.sclPin);  // must come before si5351.init()

  ok_ = si5351_.init(loadCapacitance(config.xtalLoadPf), config.xtalHz, correctionPpb);
  if (!ok_) return false;
  correctionPpb_ = correctionPpb;
  driveMa_ = driveValid(driveMa) ? driveMa : 8;
  centiHz_ = initialCentiHz;

  si5351_.output_enable(SI5351_CLK1, 0);
  si5351_.output_enable(SI5351_CLK2, 0);
  si5351_.set_clock_pwr(SI5351_CLK1, 0);
  si5351_.set_clock_pwr(SI5351_CLK2, 0);

  // The first set_freq() on a clock also enables it, so set the drive and turn the output
  // off afterwards. The PLL keeps running between transmissions, for thermal stability.
  si5351_.set_freq(centiHz_, SI5351_CLK0);
  si5351_.drive_strength(SI5351_CLK0, driveEnum(driveMa_));
  si5351_.output_enable(SI5351_CLK0, 0);
  outputOn_ = false;
  return true;
}

void Radio::setFrequencyCentiHz(uint64_t centiHz) {
  Guard g(mutex_);
  centiHz_ = centiHz;
  if (ok_) si5351_.set_freq(centiHz, SI5351_CLK0);
}

void Radio::setOutput(bool on) {
  Guard g(mutex_);
  outputOn_ = on;
  if (ok_) si5351_.output_enable(SI5351_CLK0, on ? 1 : 0);
}

bool Radio::outputOn() const {
  Guard g(mutex_);
  return outputOn_;
}

bool Radio::setDriveMa(int ma) {
  if (!driveValid(ma)) return false;
  Guard g(mutex_);
  driveMa_ = ma;
  if (ok_) si5351_.drive_strength(SI5351_CLK0, driveEnum(ma));
  return true;
}

int Radio::driveMa() const {
  Guard g(mutex_);
  return driveMa_;
}

void Radio::setCorrectionPpb(int32_t ppb) {
  Guard g(mutex_);
  correctionPpb_ = ppb;
  if (!ok_) return;
  si5351_.set_correction(ppb, SI5351_PLL_INPUT_XO);
  si5351_.set_freq(centiHz_, SI5351_CLK0);
}

int32_t Radio::correctionPpb() const {
  Guard g(mutex_);
  return correctionPpb_;
}

// The library treats the real crystal as XTAL * (1 + corr/1e9) and programs the dividers
// so that crystal gives the requested frequency. A nominal crystal would give this instead.
double Radio::effectiveHz(double requestedHz) const {
  return requestedHz / (1.0 + correctionPpb() / 1e9);
}
