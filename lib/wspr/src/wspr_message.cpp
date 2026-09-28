// Needs Arduino (JTEncode depends on it); left out of the host unit-test build.
#ifdef ARDUINO

#include "wspr_message.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <JTEncode.h>

namespace wspr {

static_assert(SYMBOL_COUNT == WSPR_SYMBOL_COUNT, "WSPR symbol count mismatch with JTEncode");

namespace {

// Copies at most n chars of `in`, upper-cased, and terminates `out` (which holds n + 1).
void copyUpper(char *out, const char *in, size_t n) {
  size_t i = 0;
  for (; i < n && in[i]; i++) out[i] = (char)toupper((unsigned char)in[i]);
  out[i] = '\0';
}

}  // namespace

bool Message::encode(const char *callsign, const char *locator, int powerDbm) {
  parts_ = 0;
  copyUpper(callsign_, callsign, sizeof callsign_ - 1);
  copyUpper(locator_, locator, sizeof locator_ - 1);
  powerDbm_ = powerDbm;
  if (!callsignValid(callsign) || !locatorValid(locator) || !powerValid(powerDbm)) return false;

  JTEncode encoder;
  char loc4[5];
  copyUpper(loc4, locator_, 4);
  encoder.wspr_encode(callsign_, loc4, (int8_t)powerDbm, symbols_[TYPE1]);
  snprintf(names_[TYPE1], sizeof names_[TYPE1], "T1 %s %s %d", callsign_, loc4, powerDbm);
  parts_ = 1;

  if (strlen(locator_) == 6) {
    char hashed[9];
    snprintf(hashed, sizeof hashed, "<%s>", callsign_);
    encoder.wspr_encode(hashed, locator_, (int8_t)powerDbm, symbols_[TYPE3]);
    snprintf(names_[TYPE3], sizeof names_[TYPE3], "T3 %s %s %d", hashed, locator_, powerDbm);
    parts_ = 2;
  }
  return true;
}

}  // namespace wspr

#endif  // ARDUINO
