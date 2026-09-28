// Si5351 synthesizer wrapper. Only CLK0 is used; CLK1 and CLK2 are powered down.
#pragma once

#include <stdint.h>
#include <si5351.h>

class Radio {
 public:
  // Starts I2C, initialises the chip and programs CLK0 with the output off.
  // Returns false if the Si5351 does not respond.
  bool begin(int sdaPin, int sclPin, uint32_t xtalHz, int xtalLoadPf, int32_t correctionPpb,
             int driveMa, uint64_t initialCentiHz);
  bool ok() const { return ok_; }

  // Frequency in 0.01 Hz units, as the Etherkit library expects.
  void setFrequencyCentiHz(uint64_t centiHz);

  void setOutput(bool on);
  bool outputOn() const { return outputOn_; }

  // 2, 4, 6 or 8 mA. Returns false for other values.
  bool setDriveMa(int ma);
  int driveMa() const { return driveMa_; }

  // Applies the correction and re-programs the current frequency so it takes effect now.
  void setCorrectionPpb(int32_t ppb);
  int32_t correctionPpb() const { return correctionPpb_; }

  // Output frequency if the crystal were exactly nominal, given the current correction.
  double effectiveHz(double requestedHz) const;

 private:
  Si5351 si5351_;
  bool ok_ = false;
  bool outputOn_ = false;
  int driveMa_ = 8;
  int32_t correctionPpb_ = 0;
  uint64_t centiHz_ = 0;
};
