// Si5351 synthesizer as the beacon's transmitter. Only CLK0 is used; CLK1 and CLK2 are
// powered down. Thread-safe: the beacon task and loop() both drive it.
#pragma once

#include <stdint.h>

#include <mutex>

#include <si5351.h>
#include <wspr_beacon.h>

class Radio : public wspr::Transmitter {
 public:
  struct Config {
    int sdaPin;
    int sclPin;
    uint32_t xtalHz;
    int xtalLoadPf;  // 6, 8 or 10
  };

  // Starts I2C, initialises the chip and programs CLK0 with the output off.
  // Returns false if the Si5351 does not respond.
  bool begin(const Config &config, int32_t correctionPpb, int driveMa, uint64_t initialCentiHz);

  // wspr::Transmitter
  bool ready() const override { return ok_; }  // only written by begin()
  void setFrequencyCentiHz(uint64_t centiHz) override;
  void setOutput(bool on) override;

  bool outputOn() const;

  // 2, 4, 6 or 8 mA. Returns false for other values.
  static bool driveValid(int ma) { return ma == 2 || ma == 4 || ma == 6 || ma == 8; }
  bool setDriveMa(int ma);
  int driveMa() const;

  // Applies the correction and re-programs the current frequency so it takes effect now.
  void setCorrectionPpb(int32_t ppb);
  int32_t correctionPpb() const;

  // Output frequency if the crystal were exactly nominal, given the current correction.
  double effectiveHz(double requestedHz) const;

 private:
  using Guard = std::lock_guard<std::mutex>;

  Si5351 si5351_;
  mutable std::mutex mutex_;
  bool ok_ = false;
  bool outputOn_ = false;
  int driveMa_ = 8;
  int32_t correctionPpb_ = 0;
  uint64_t centiHz_ = 0;
};
