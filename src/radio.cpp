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

bool Radio::begin(int sdaPin, int sclPin, uint32_t xtalHz, int xtalLoadPf, int32_t correctionPpb,
                  int driveMa, uint64_t initialCentiHz) {
  Wire.begin(sdaPin, sclPin);  // must come before si5351.init()

  correctionPpb_ = correctionPpb;
  ok_ = si5351_.init(loadCapacitance(xtalLoadPf), xtalHz, correctionPpb);
  if (!ok_) return false;

  si5351_.set_correction(correctionPpb_, SI5351_PLL_INPUT_XO);

  si5351_.output_enable(SI5351_CLK1, 0);
  si5351_.output_enable(SI5351_CLK2, 0);
  si5351_.set_clock_pwr(SI5351_CLK1, 0);
  si5351_.set_clock_pwr(SI5351_CLK2, 0);

  // The first set_freq() on a clock also enables it, so set drive and turn the output off after.
  // The PLL keeps running between transmissions, which helps thermal stability.
  setFrequencyCentiHz(initialCentiHz);
  setDriveMa(driveMa);
  setOutput(false);
  return true;
}

void Radio::setFrequencyCentiHz(uint64_t centiHz) {
  centiHz_ = centiHz;
  if (ok_) si5351_.set_freq(centiHz, SI5351_CLK0);
}

void Radio::setOutput(bool on) {
  outputOn_ = on;
  if (ok_) si5351_.output_enable(SI5351_CLK0, on ? 1 : 0);
}

bool Radio::setDriveMa(int ma) {
  if (ma != 2 && ma != 4 && ma != 6 && ma != 8) return false;
  driveMa_ = ma;
  if (ok_) si5351_.drive_strength(SI5351_CLK0, driveEnum(ma));
  return true;
}

void Radio::setCorrectionPpb(int32_t ppb) {
  correctionPpb_ = ppb;
  if (!ok_) return;
  si5351_.set_correction(ppb, SI5351_PLL_INPUT_XO);
  setFrequencyCentiHz(centiHz_);
}

// The library treats the real crystal as XTAL * (1 + corr/1e9) and programs the dividers
// so that crystal gives the requested frequency. A nominal crystal would give this instead.
double Radio::effectiveHz(double requestedHz) const {
  return requestedHz / (1.0 + correctionPpb_ / 1e9);
}
