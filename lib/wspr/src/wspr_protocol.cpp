#include "wspr_protocol.h"

#include <ctype.h>
#include <string.h>

namespace wspr {

namespace {

bool isAlnum(char c) { return isalnum((unsigned char)c) != 0; }
bool isDigit(char c) { return isdigit((unsigned char)c) != 0; }
bool isAlpha(char c) { return isalpha((unsigned char)c) != 0; }

bool inRange(char c, char lo, char hi) {
  c = (char)toupper((unsigned char)c);
  return c >= lo && c <= hi;
}

}  // namespace

bool callsignValid(const char *call) {
  size_t len = strlen(call);
  if (len < 3 || len > 6) return false;
  for (size_t i = 0; i < len; i++)
    if (!isAlnum(call[i])) return false;

  // Same alignment as the encoder: a digit in second place is shifted right by one,
  // after which the third character must be the digit and the rest letters.
  size_t shift = (isDigit(call[1]) && isAlpha(call[2])) ? 1 : 0;
  if (len + shift > 6) return false;
  char aligned[6];
  memset(aligned, ' ', sizeof aligned);
  memcpy(aligned + shift, call, len);

  if (!isDigit(aligned[2])) return false;
  for (int i = 3; i < 6; i++)
    if (aligned[i] != ' ' && !isAlpha(aligned[i])) return false;
  return true;
}

bool locatorValid(const char *loc) {
  size_t len = strlen(loc);
  if (len != 4 && len != 6) return false;
  if (!inRange(loc[0], 'A', 'R') || !inRange(loc[1], 'A', 'R')) return false;
  if (!isDigit(loc[2]) || !isDigit(loc[3])) return false;
  return len == 4 || (inRange(loc[4], 'A', 'X') && inRange(loc[5], 'A', 'X'));
}

}  // namespace wspr
