// A station's WSPR transmissions, encoded to channel symbols (with Etherkit JTEncode).
//   4-char locator: one Type 1 message (callsign, locator, power), sent every time.
//   6-char locator: Type 1 (callsign, first 4 chars, power) and
//                   Type 3 (<callsign> hash, full 6-char locator, power). Receivers can only
//                   resolve the Type 3 hash after they have decoded a Type 1 from us.
#pragma once

#include <stdint.h>

#include "wspr_protocol.h"

namespace wspr {

class Message {
 public:
  static constexpr int MAX_PARTS = 2;
  static constexpr int TYPE1 = 0;  // part indices
  static constexpr int TYPE3 = 1;

  // Returns false, leaving the message invalid, if a field can't be sent
  // (see callsignValid, locatorValid and powerValid).
  bool encode(const char *callsign, const char *locator, int powerDbm);
  bool valid() const { return parts_ > 0; }

  // The fields as given to encode(), upper-cased (kept even if invalid, for display).
  const char *callsign() const { return callsign_; }
  const char *locator() const { return locator_; }
  int powerDbm() const { return powerDbm_; }

  int parts() const { return parts_; }  // 0 (invalid), 1, or 2 when Type 3 is available
  const char *partName(int part) const { return part >= 0 && part < parts_ ? names_[part] : ""; }
  uint8_t symbol(int part, int index) const { return symbols_[part][index]; }

 private:
  uint8_t symbols_[MAX_PARTS][SYMBOL_COUNT] = {};
  char names_[MAX_PARTS][24] = {};  // e.g. "T3 <K1ABC> FN42AX 23"
  char callsign_[12] = {};
  char locator_[8] = {};
  int powerDbm_ = 0;
  int parts_ = 0;
};

}  // namespace wspr
