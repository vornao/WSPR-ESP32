// 20m WSPR beacon (ESP32-C3 + Si5351).
// UTC comes from NTP over WiFi. Transmits a WSPR Type 1 message (call, 4-char locator, dBm)
// on Si5351 CLK0 starting 1 s after an even UTC minute, every N 2-minute slots.
// Always run through a 20m low-pass filter; test into a dummy load first.
//
// Settings: include/config.h, WiFi credentials: include/secrets.h.
//
// Modules:
//   radio       Si5351 on CLK0: frequency, output, drive, correction
//   wspr        message encoding, tone and symbol timing
//   time_sync   WiFi + NTP, UTC clock
//   beacon      slot scheduling and symbol keying
//   status_led  LED: solid while transmitting, blinking with the test carrier
//   station     control actions shared by the console and the web interface
//   console     serial command menu
//   web_ui      web control page and JSON API at http://wspr.local/
//   settings    runtime settings kept in flash across restarts
//   ota         firmware updates over WiFi (pio run -e esp32c3-ota -t upload)

#include <Arduino.h>
#include <si5351.h>

#include "beacon.h"
#include "config.h"
#include "console.h"
#include "ota.h"
#include "radio.h"
#include "settings.h"
#include "station.h"
#include "status_led.h"
#include "time_sync.h"
#include "web_ui.h"
#include "wspr.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.h.example to include/secrets.h and set your WiFi credentials"
#endif
#ifndef WEB_PASSWORD
#define WEB_PASSWORD ""  // no login on the web interface
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""  // no password for WiFi firmware updates
#endif

Radio radio;
wspr::Message message;
TimeSync timeSync;
StatusLed led;
Beacon beacon(radio, message, timeSync);
Station station(radio, beacon);
Console console(station, radio, beacon, timeSync, message, led);
WebUi web(station, radio, beacon, timeSync, message);
Ota ota;

void setup() {
  Serial.begin(config::SERIAL_BAUD);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);  // wait for USB CDC, but don't hang
  delay(500);

  led.begin(config::LED_PIN, config::LED_ACTIVE_LOW, config::LED_BLINK_MS);

  Serial.println();
  Serial.println("=== WSPR beacon 20m ===");
  Serial.printf("ESP32-C3 | I2C SDA=%d SCL=%d | xtal %lu Hz\n", config::SDA_PIN, config::SCL_PIN,
                (unsigned long)config::XTAL_FREQ_HZ);
  Serial.println("Use a low-pass filter. Test into a dummy load first.");

  bool fromFlash = false;
  Settings saved = settings::load(&fromFlash);
  Serial.printf("Settings: %s\n", fromFlash ? "loaded from flash" : "defaults from config.h");

  bool radioOk = radio.begin(config::SDA_PIN, config::SCL_PIN, config::XTAL_FREQ_HZ,
                             config::XTAL_LOAD_PF, saved.correctionPpb, saved.driveMa,
                             saved.centerHz * SI5351_FREQ_MULT);
  if (radioOk) {
    Serial.println("Si5351 found.");
    console.printFreq((double)saved.centerHz);
  } else {
    Serial.println("ERROR: Si5351 not found at 0x60. Check SDA/SCL wiring, power and pull-ups.");
  }

  if (message.encode(config::CALLSIGN, config::LOCATOR, config::POWER_DBM)) {
    for (int i = 0; i < message.parts(); i++)
      Serial.printf("WSPR message %d/%d: %s dBm\n", i + 1, message.parts(), message.partName(i));
  } else {
    Serial.printf("WSPR message invalid: call '%s' locator '%s'. Fix them in config.h.\n",
                  config::CALLSIGN, config::LOCATOR);
  }

  beacon.setup(saved.centerHz, config::TX_EVERY_N_SLOTS, config::TX_RANDOM_OFFSET_HZ);
  station.apply(saved);  // interval, message mode, beacon on/off (after the message is encoded)
  timeSync.begin(WIFI_SSID, WIFI_PASSWORD, config::NTP_SERVER);
  web.begin(config::MDNS_NAME, WEB_PASSWORD);
  ota.begin(
      config::MDNS_NAME, OTA_PASSWORD,
      [] {  // update starting: get off the air
        station.cancel();
        if (station.carrierOn()) station.setCarrier(false);
        beacon.setPaused(true);
        radio.setOutput(false);
      },
      [] { beacon.setPaused(false); });

  console.printHelp();
}

void loop() {
  console.poll();
  timeSync.service();
  web.service();
  beacon.service();
  ota.service(timeSync.wifiConnected());

  if (beacon.transmitting()) led.setMode(StatusLed::Mode::Solid);
  else if (station.carrierOn()) led.setMode(StatusLed::Mode::Blink);
  else led.setMode(StatusLed::Mode::Off);
  led.update();

  delay(1);
}
