#include "wspr.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <JTEncode.h>
#include <si5351.h>

namespace wspr {

static_assert(SYMBOL_COUNT == WSPR_SYMBOL_COUNT, "WSPR symbol count mismatch with JTEncode");

int symbolAt(int64_t elapsedUs) {
  return (int)(elapsedUs * 12000LL / 8192000000LL);
}

uint64_t toneCentiHz(uint64_t baseHz, uint8_t tone) {
  return baseHz * SI5351_FREQ_MULT + (uint64_t)llround(tone * TONE_SPACING_HZ * SI5351_FREQ_MULT);
}

bool locatorValid(const char *loc) {
  size_t len = strlen(loc);
  if (len != 4 && len != 6) return false;
  if (loc[0] < 'A' || loc[0] > 'R' || loc[1] < 'A' || loc[1] > 'R') return false;
  if (!isdigit((unsigned char)loc[2]) || !isdigit((unsigned char)loc[3])) return false;
  for (size_t i = 4; i < len; i++) {
    char c = (char)toupper((unsigned char)loc[i]);
    if (c < 'A' || c > 'X') return false;
  }
  return true;
}

bool Message::encode(const char *callsign, const char *locator, int8_t powerDbm) {
  size_t callLen = strlen(callsign);
  valid_ = locatorValid(locator) && callLen >= 3 && callLen <= 6;
  parts_ = 0;
  if (!valid_) return false;

  char loc4[5];
  memcpy(loc4, locator, 4);
  loc4[4] = 0;

  JTEncode encoder;
  encoder.wspr_encode(callsign, loc4, powerDbm, symbols_[0]);
  snprintf(names_[0], sizeof names_[0], "T1 %s %s %d", callsign, loc4, powerDbm);
  parts_ = 1;

  if (strlen(locator) == 6) {
    char loc6[7];
    for (int i = 0; i < 6; i++) loc6[i] = (char)toupper((unsigned char)locator[i]);
    loc6[6] = 0;
    char hashed[10];
    snprintf(hashed, sizeof hashed, "<%s>", callsign);
    encoder.wspr_encode(hashed, loc6, powerDbm, symbols_[1]);
    snprintf(names_[1], sizeof names_[1], "T3 %s %s %d", hashed, loc6, powerDbm);
    parts_ = 2;
  }
  return true;
}

}  // namespace wspr
