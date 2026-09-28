// WSPR protocol: message encoding, tone frequencies and timing.
#pragma once

#include <stdint.h>

namespace wspr {

constexpr int SYMBOL_COUNT = 162;
constexpr double TONE_SPACING_HZ = 12000.0 / 8192.0;  // 1.46484375 Hz
constexpr int64_t SLOT_S = 120;                       // transmissions start on even UTC minutes
constexpr int64_t START_OFFSET_S = 1;                 // ...1 s into the slot

// Index of the symbol being sent `elapsedUs` after the start (symbol length 8192/12000 s).
int symbolAt(int64_t elapsedUs);

// Frequency of `tone` (0-3) above `baseHz`, in 0.01 Hz units.
uint64_t toneCentiHz(uint64_t baseHz, uint8_t tone);

// 4-char (IO91) or 6-char (IO91WM, subsquare letters in either case) Maidenhead locator.
bool locatorValid(const char *locator);

// The beacon's WSPR transmissions, encoded to channel symbols.
//   4-char locator: one Type 1 message (callsign, locator, power), sent every time.
//   6-char locator: Type 1 (callsign, first 4 chars, power) alternating with
//                   Type 3 (<callsign> hash, full 6-char locator, power). Receivers can only
//                   resolve the Type 3 hash after they have decoded a Type 1 from us.
class Message {
 public:
  static constexpr int MAX_PARTS = 2;

  // Returns false (and leaves the message invalid) if the callsign or locator is unusable.
  bool encode(const char *callsign, const char *locator, int8_t powerDbm);
  bool valid() const { return valid_; }

  int parts() const { return parts_; }                     // 1 or 2 alternating transmissions
  const char *partName(int part) const { return names_[part]; }
  uint8_t symbol(int part, int index) const { return symbols_[part][index]; }

 private:
  uint8_t symbols_[MAX_PARTS][SYMBOL_COUNT] = {};
  char names_[MAX_PARTS][32] = {};
  int parts_ = 0;
  bool valid_ = false;
};

}  // namespace wspr
