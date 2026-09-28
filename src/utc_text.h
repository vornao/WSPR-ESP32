// "YYYY-MM-DD hh:mm:ss UTC" for log output, without heap allocation:
//   Serial.printf("next TX %s\n", utcText(t).s);
#pragma once

#include <stdio.h>
#include <time.h>

struct UtcText {
  char s[24];
};

inline UtcText utcText(time_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  UtcText out;
  snprintf(out.s, sizeof out.s, "%04d-%02d-%02d %02d:%02d:%02d UTC", tm.tm_year + 1900, tm.tm_mon + 1,
           tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return out;
}
