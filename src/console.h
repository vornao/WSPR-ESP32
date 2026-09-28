// Serial command menu for tuning and controlling the beacon.
#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "beacon.h"
#include "radio.h"
#include "station.h"
#include "status_led.h"
#include "time_sync.h"
#include "wspr.h"

class Console {
 public:
  Console(Station &station, Radio &radio, Beacon &beacon, const TimeSync &time,
          const wspr::Message &message, StatusLed &led);

  // Call often from loop(): reads serial input and runs complete commands.
  void poll();

  void printHelp();

  // Prints the requested frequency and the effective one with correction applied.
  void printFreq(double requestedHz);

 private:
  void handle(String line);
  void printState();
  void printNextTx();
  void printHistory();

  // Prints why a Station action failed (with `usage` for bad input). Returns true on success.
  bool report(Station::Result r, const char *usage);

  void toneTest();

  Station &station_;
  Radio &radio_;
  Beacon &beacon_;
  const TimeSync &time_;
  const wspr::Message &message_;
  StatusLed &led_;

  String line_;
  uint64_t stepHz_;
};
