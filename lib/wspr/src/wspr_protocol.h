// WSPR protocol: slot and symbol timing, tone frequencies, and validation of the
// message fields. Plain C++ with no Arduino or ESP-IDF dependency, so it is
// unit-tested on the host (see test/).
#pragma once

#include <stdint.h>

namespace wspr {

// ---------- timing ----------

constexpr int SYMBOL_COUNT = 162;
constexpr int64_t SLOT_S = 120;        // transmissions start on even UTC minutes...
constexpr int64_t START_OFFSET_S = 1;  // ...1 s into the slot

// UTC second at which `slot` (a count of 2-minute slots since the epoch) starts transmitting.
constexpr int64_t slotStartS(int64_t slot) { return slot * SLOT_S + START_OFFSET_S; }

// A symbol lasts 8192/12000 s. Integer maths keeps the timing exact over the whole message.
// Index of the symbol on air `elapsedUs` after symbol 0 started.
constexpr int symbolAt(int64_t elapsedUs) { return (int)(elapsedUs * 12000 / 8192000000LL); }

// First microsecond of symbol `index` (rounded up, so symbolAt(symbolStartUs(i)) == i).
constexpr int64_t symbolStartUs(int index) { return ((int64_t)index * 8192000000LL + 11999) / 12000; }

// ---------- frequencies ----------

constexpr uint64_t CENTI_HZ = 100;  // frequencies below are in 0.01 Hz units, like most Si5351 drivers
constexpr double TONE_SPACING_HZ = 12000.0 / 8192.0;  // 1.46484375 Hz
constexpr int WINDOW_HZ = 200;                        // width of the WSPR sub-band

// The four tones span 4.4 Hz above tone 0, so a random offset of up to this many Hz either
// side of the window centre keeps the whole signal inside the window.
constexpr int MAX_RANDOM_OFFSET_HZ = WINDOW_HZ / 2 - 5;

// Frequency of `tone` (0-3) above `baseHz`, in 0.01 Hz, rounded to the nearest 0.01 Hz.
// The spacing is exactly 1171875/8000 centi-Hz.
constexpr uint64_t toneCentiHz(uint64_t baseHz, uint8_t tone) {
  return baseHz * CENTI_HZ + ((uint64_t)tone * 1171875 + 4000) / 8000;
}

// ---------- message fields ----------

// Reportable power: 0..60 dBm, ending in 0, 3 or 7.
constexpr bool powerValid(int dbm) {
  return dbm >= 0 && dbm <= 60 && (dbm % 10 == 0 || dbm % 10 == 3 || dbm % 10 == 7);
}

// A callsign a Type 1 message can carry: 3 to 6 letters and digits, with a digit in the
// third position once aligned (K1ABC is sent as " K1ABC"). Either case. Compound
// callsigns (with '/') need Type 2 messages, which this beacon does not send.
bool callsignValid(const char *callsign);

// 4-char (JN61) or 6-char (JN61fv) Maidenhead locator, either case.
bool locatorValid(const char *locator);

}  // namespace wspr
