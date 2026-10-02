// Serial command menu for tuning and controlling the beacon. Also logs transmissions.
#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <wspr_beacon.h>

#include "radio.h"
#include "station.h"
#include "time_sync.h"
#include "wifi_link.h"

class Console {
 public:
  Console(Station &station, Radio &radio, wspr::Beacon &beacon, const TimeSync &time,
          const WifiLink &wifi, const wspr::Message &message, uint64_t stepHz);

  // Call often from loop(): runs complete commands and logs transmissions and test tones.
  void service();

  void printHelp();

  // Prints the requested frequency and the effective one with correction applied.
  void printFreq(double requestedHz);

 private:
  void handle(String line);
  void logEvents();
  void printState();
  void printNextTx();
  void printHistory();

  // Prints why a Station action failed (with `usage` for bad input). Returns true on success.
  bool report(Station::Result r, const char *usage);

  Station &station_;
  Radio &radio_;
  wspr::Beacon &beacon_;
  const TimeSync &time_;
  const WifiLink &wifi_;
  const wspr::Message &message_;

  String line_;
  uint64_t stepHz_;
  uint32_t seenChanges_ = 0;
  int seenTone_ = -1;
};
