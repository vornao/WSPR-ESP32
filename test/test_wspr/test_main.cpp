// Host tests for the portable part of lib/wspr: pio test -e native
#include <unity.h>

#include <wspr_protocol.h>
#include <wspr_schedule.h>

using namespace wspr;

void setUp() {}
void tearDown() {}

// ---------- timing ----------

void test_symbol_timing() {
  TEST_ASSERT_EQUAL(0, symbolAt(0));
  TEST_ASSERT_EQUAL(0, symbolAt(682666));     // symbol 0 lasts 682.666.. ms
  TEST_ASSERT_EQUAL(1, symbolAt(682667));
  TEST_ASSERT_EQUAL(161, symbolAt(symbolStartUs(162) - 1));
  TEST_ASSERT_EQUAL(162, symbolAt(symbolStartUs(162)));
  TEST_ASSERT_EQUAL_INT64(110592000, symbolStartUs(162));  // 162 * 8192 / 12000 s, exactly
}

void test_symbol_start_round_trips() {
  for (int i = 0; i <= SYMBOL_COUNT; i++) {
    TEST_ASSERT_EQUAL(i, symbolAt(symbolStartUs(i)));
    if (i > 0) TEST_ASSERT_EQUAL(i - 1, symbolAt(symbolStartUs(i) - 1));
  }
}

void test_slot_start() {
  TEST_ASSERT_EQUAL_INT64(1, slotStartS(0));
  TEST_ASSERT_EQUAL_INT64(121, slotStartS(1));
}

void test_tone_frequencies() {
  const uint64_t base = 14097100;
  TEST_ASSERT_EQUAL_UINT64(1409710000, toneCentiHz(base, 0));
  TEST_ASSERT_EQUAL_UINT64(1409710146, toneCentiHz(base, 1));  // 1.4648 Hz
  TEST_ASSERT_EQUAL_UINT64(1409710293, toneCentiHz(base, 2));  // 2.9297 Hz
  TEST_ASSERT_EQUAL_UINT64(1409710439, toneCentiHz(base, 3));  // 4.3945 Hz
}

void test_random_offset_keeps_tones_in_window() {
  // Highest tone at the largest offset still inside centre + 100 Hz.
  TEST_ASSERT_TRUE(MAX_RANDOM_OFFSET_HZ + 3 * TONE_SPACING_HZ <= WINDOW_HZ / 2);
}

// ---------- message fields ----------

void test_power() {
  const int valid[] = {0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40, 43, 47, 50, 53, 57, 60};
  for (int dbm : valid) TEST_ASSERT_TRUE_MESSAGE(powerValid(dbm), "valid power rejected");
  const int invalid[] = {-3, 1, 5, 21, 25, 61, 63, 100};
  for (int dbm : invalid) TEST_ASSERT_FALSE_MESSAGE(powerValid(dbm), "invalid power accepted");
}

void test_callsign_valid() {
  const char *valid[] = {"K1ABC", "k1abc", "G4XYZ", "2E0ABC", "DL1AB", "AA1AA", "W1AW", "9A1AA", "K12AB", "4X4AB", "S51ABC", "A1B"};
  for (const char *c : valid) TEST_ASSERT_TRUE_MESSAGE(callsignValid(c), c);
}

void test_callsign_invalid() {
  const char *invalid[] = {"",        "K1",      "ABCDEF",  "K1ABCD",  // 7 chars once aligned
                           "N0CALL",  "DL1ABCD", "K1A-C",   "PA/K1ABC", "K1ABC/P", "K1 AB",
                           "KA1A1",   "1234"};
  for (const char *c : invalid) TEST_ASSERT_FALSE_MESSAGE(callsignValid(c), c);
}

void test_locator() {
  TEST_ASSERT_TRUE(locatorValid("JN61"));
  TEST_ASSERT_TRUE(locatorValid("jn61"));
  TEST_ASSERT_TRUE(locatorValid("JN61FV"));
  TEST_ASSERT_TRUE(locatorValid("JN61fv"));
  TEST_ASSERT_TRUE(locatorValid("RR99XX"));
  TEST_ASSERT_TRUE(locatorValid("AA00AA"));
  TEST_ASSERT_FALSE(locatorValid(""));
  TEST_ASSERT_FALSE(locatorValid("JN6"));
  TEST_ASSERT_FALSE(locatorValid("JN61F"));
  TEST_ASSERT_FALSE(locatorValid("SN61"));    // field letters A-R
  TEST_ASSERT_FALSE(locatorValid("JN6A"));
  TEST_ASSERT_FALSE(locatorValid("JN61YA"));  // subsquare letters A-X
  TEST_ASSERT_FALSE(locatorValid("JN61FV00"));
}

// ---------- schedule ----------

void test_schedule_first_slot_then_every_n() {
  SlotSchedule s;
  s.setEveryN(3);
  TEST_ASSERT_TRUE(s.due(100));  // nothing sent yet: the first slot is due
  s.markTransmitted(100);
  TEST_ASSERT_FALSE(s.due(101));
  TEST_ASSERT_FALSE(s.due(102));
  TEST_ASSERT_TRUE(s.due(103));
  TEST_ASSERT_TRUE(s.due(110));  // overdue slots are due too
  TEST_ASSERT_EQUAL_INT64(103, s.nextDue(101));
  TEST_ASSERT_EQUAL_INT64(105, s.nextDue(105));
}

void test_schedule_every_slot() {
  SlotSchedule s;
  TEST_ASSERT_TRUE(s.setEveryN(1));
  s.markTransmitted(50);
  TEST_ASSERT_TRUE(s.due(51));
  TEST_ASSERT_EQUAL_INT64(51, s.nextDue(51));
}

void test_schedule_disabled() {
  SlotSchedule s;
  s.setEnabled(false);
  TEST_ASSERT_FALSE(s.due(10));
  TEST_ASSERT_EQUAL_INT64(-1, s.nextDue(10));
}

void test_schedule_request_overrides() {
  SlotSchedule s;
  s.setEveryN(5);
  s.markTransmitted(10);
  s.request();
  TEST_ASSERT_TRUE(s.due(11));  // requested: the next slot, whatever the interval
  s.markTransmitted(11);
  TEST_ASSERT_FALSE(s.requested());
  TEST_ASSERT_EQUAL_INT64(16, s.nextDue(12));  // the interval restarts from the requested one

  s.setEnabled(false);
  s.request();
  TEST_ASSERT_EQUAL_INT64(12, s.nextDue(12));  // a request works with the schedule off
  s.clearRequest();
  TEST_ASSERT_EQUAL_INT64(-1, s.nextDue(12));
}

void test_schedule_interval_limits() {
  SlotSchedule s;
  TEST_ASSERT_FALSE(s.setEveryN(0));
  TEST_ASSERT_FALSE(s.setEveryN(SlotSchedule::MAX_EVERY_N + 1));
  TEST_ASSERT_EQUAL(1, s.everyN());
  TEST_ASSERT_TRUE(s.setEveryN(SlotSchedule::MAX_EVERY_N));
  TEST_ASSERT_EQUAL(SlotSchedule::MAX_EVERY_N, s.everyN());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_symbol_timing);
  RUN_TEST(test_symbol_start_round_trips);
  RUN_TEST(test_slot_start);
  RUN_TEST(test_tone_frequencies);
  RUN_TEST(test_random_offset_keeps_tones_in_window);
  RUN_TEST(test_power);
  RUN_TEST(test_callsign_valid);
  RUN_TEST(test_callsign_invalid);
  RUN_TEST(test_locator);
  RUN_TEST(test_schedule_first_slot_then_every_n);
  RUN_TEST(test_schedule_every_slot);
  RUN_TEST(test_schedule_disabled);
  RUN_TEST(test_schedule_request_overrides);
  RUN_TEST(test_schedule_interval_limits);
  return UNITY_END();
}
