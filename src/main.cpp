// WSPR beacon (ESP32-C3 + Si5351).
// UTC comes from NTP over WiFi. Transmits WSPR on Si5351 CLK0 starting 1 s after an even
// UTC minute, every N 2-minute slots. Always run through a low-pass filter for the band;
// test into a dummy load first.
//
// Settings: include/config.h (defaults) and include/config_local.h (yours);
// WiFi credentials and passwords: include/secrets.h.
//
// lib/wspr is the reusable part: protocol, scheduling and the beacon engine. This file
// wires it to the hardware and to the user interfaces:
//   radio       Si5351 on CLK0 (the beacon's wspr::Transmitter)
//   time_sync   WiFi + NTP (the beacon's wspr::Clock)
//   station     control actions shared by the console and the web interface
//   console     serial command menu and TX log
//   web_ui      web control page and JSON API at http://wspr.local/
//   settings    runtime settings kept in flash across restarts
//   ota         firmware updates over WiFi (pio run -e esp32c3-ota -t upload)
//   status_led  LED: solid while transmitting, blinking while a test output is on

#include <Arduino.h>
#include <wspr.h>

#include "config.h"
#include "console.h"
#include "ota.h"
#include "radio.h"
#include "settings.h"
#include "station.h"
#include "status_led.h"
#include "time_sync.h"
#include "web_ui.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.h.example to include/secrets.h and set your WiFi credentials"
#endif
#ifndef WEB_PASSWORD
#define WEB_PASSWORD ""  // no login on the web interface
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""  // no firmware updates over WiFi
#endif

// Configuration mistakes stop the build rather than put a bad signal on the air.
static_assert(wspr::powerValid(config::POWER_DBM), "CFG_POWER_DBM must be 0..60 dBm, ending in 0, 3 or 7");
static_assert(limits::centerHz(config::CENTER_FREQ_HZ), "CFG_CENTER_FREQ_HZ must be 8 kHz .. 160 MHz");
static_assert(limits::correctionPpb(config::CORRECTION_PPB), "CFG_CORRECTION_PPB must be within +/- 1000000");
static_assert(limits::driveMa(config::DRIVE_MA), "CFG_DRIVE_MA must be 2, 4, 6 or 8");
static_assert(limits::everyNSlots(config::TX_EVERY_N_SLOTS), "CFG_TX_EVERY_N_SLOTS must be 1 .. 30");
static_assert(config::TX_RANDOM_OFFSET_HZ >= 0 && config::TX_RANDOM_OFFSET_HZ <= wspr::MAX_RANDOM_OFFSET_HZ,
              "CFG_TX_RANDOM_OFFSET_HZ must be 0 .. 95 to stay inside the WSPR window");
static_assert(config::XTAL_LOAD_PF == 6 || config::XTAL_LOAD_PF == 8 || config::XTAL_LOAD_PF == 10,
              "CFG_XTAL_LOAD_PF must be 6, 8 or 10");

namespace {

Radio radio;
wspr::Message message;
TimeSync timeSync;
wspr::Beacon beacon(radio, message, timeSync);
Station station(radio, beacon, config::TONE_DWELL_MS);
Console console(station, radio, beacon, timeSync, message, config::STEP_HZ);
WebUi web(station, radio, beacon, timeSync, message);
Ota ota;
StatusLed led;

void waitForSerial() {
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);  // wait for USB CDC, but don't hang
  delay(500);
}

}  // namespace

void setup() {
  Serial.begin(config::SERIAL_BAUD);
  waitForSerial();
  led.begin(config::LED_PIN, config::LED_ACTIVE_LOW, config::LED_BLINK_MS);

  Serial.println();
  Serial.println("=== WSPR beacon ===");
  Serial.printf("I2C SDA=%d SCL=%d | xtal %lu Hz\n", config::SDA_PIN, config::SCL_PIN,
                (unsigned long)config::XTAL_FREQ_HZ);
  Serial.println("Use a low-pass filter. Test into a dummy load first.");

  bool fromFlash = false;
  Settings saved = settings::load(&fromFlash);
  Serial.printf("Settings: %s\n", fromFlash ? "loaded from flash" : "defaults from config.h");

  Radio::Config hw{config::SDA_PIN, config::SCL_PIN, config::XTAL_FREQ_HZ, config::XTAL_LOAD_PF};
  if (radio.begin(hw, saved.correctionPpb, saved.driveMa, saved.centerHz * wspr::CENTI_HZ)) {
    Serial.println("Si5351 found.");
    console.printFreq((double)saved.centerHz);
  } else {
    Serial.println("ERROR: Si5351 not found at 0x60. Check SDA/SCL wiring, power and pull-ups.");
  }

  if (message.encode(config::CALLSIGN, config::LOCATOR, config::POWER_DBM)) {
    for (int i = 0; i < message.parts(); i++)
      Serial.printf("WSPR message %d/%d: %s dBm\n", i + 1, message.parts(), message.partName(i));
  } else {
    Serial.printf("ERROR: can't send callsign '%s' / locator '%s' in WSPR; nothing will be transmitted. "
                  "Fix CFG_CALLSIGN / CFG_LOCATOR.\n",
                  config::CALLSIGN, config::LOCATOR);
  }

  beacon.setRandomOffsetHz(config::TX_RANDOM_OFFSET_HZ);
  station.apply(saved);  // after the message is encoded: the message mode depends on it
  if (!beacon.begin()) Serial.println("ERROR: could not start the beacon task");

  timeSync.begin(WIFI_SSID, WIFI_PASSWORD, config::NTP_SERVER);
  web.begin(config::MDNS_NAME, WEB_PASSWORD, config::WEB_CHECK_HOST);
  ota.begin(
      config::MDNS_NAME, OTA_PASSWORD, [] { station.suspend(); }, [] { station.resume(); });

  console.printHelp();
}

void loop() {
  console.service();
  timeSync.service();
  web.service();
  station.service();
  ota.service(timeSync.wifiConnected());

  if (beacon.transmitting()) led.setMode(StatusLed::Mode::Solid);
  else if (station.testMode() != Station::TestMode::Off) led.setMode(StatusLed::Mode::Blink);
  else led.setMode(StatusLed::Mode::Off);
  led.update();

  delay(1);
}
