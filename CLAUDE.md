# CLAUDE.md

WSPR beacon firmware for an ESP32-C3 driving an Si5351 clock generator (PlatformIO, Arduino
framework). README.md is the user-facing manual; this file covers what you need to change the code.

## Commands

`pio` is not on PATH here. Use `~/.platformio/penv/bin/pio`.

```sh
pio run                            # build firmware (env esp32c3)
pio run -t upload                  # flash over USB (/dev/cu.usbmodem*)
pio run -e esp32c3-ota -t upload   # flash over WiFi (needs OTA password, see README)
pio device monitor                 # serial console, 115200 baud
pio test -e native                 # host unit tests for lib/wspr (no board needed)
```

After any change to `lib/wspr/src/wspr_protocol.*` or `wspr_schedule.*`, run `pio test -e native`.
After any firmware change, run `pio run` to check it builds. Don't flash or OTA-upload unless asked:
the board is wired to a transmitter.

## Layout

- `lib/wspr/` is the reusable WSPR engine (namespace `wspr`). It must not depend on anything in `src/`.
  - `wspr_protocol.*` and `wspr_schedule.*` are **portable C++11**: no Arduino or ESP-IDF headers.
    They are compiled by the `native` test env, so keep them that way.
  - `wspr_message.*` uses Etherkit JTEncode (Arduino).
  - `wspr_beacon.*` is a FreeRTOS task that keys a `wspr::Transmitter` from a `wspr::Clock` (ESP32 only).
- `src/` is this firmware. `main.cpp` only wires objects together and holds the config `static_assert`s.
  - `radio` implements `wspr::Transmitter` (Si5351 CLK0); `time_sync` implements `wspr::Clock` (NTP).
  - `wifi_link` owns the WiFi station. `setup()` starts it; modules that need the network take a
    `const WifiLink &` instead of calling `WiFi` themselves. `loop()` starts `TimeSync` once it connects.
  - `station` holds every control action. The console and web UI both call it; neither changes the
    radio or beacon directly.
  - `settings` persists runtime settings to NVS (`Preferences`); `limits::` holds the accepted ranges.
  - `web_page.h` is the whole web UI as one raw-string HTML/CSS/JS literal served from flash.
  - `json_writer.h` is a tiny allocation-free JSON writer used by the web API.
- `test/test_wspr/test_main.cpp` holds the Unity tests for the portable part of `lib/wspr`.

## Configuration

- `include/config.h` holds every `CFG_*` default, wrapped in `#ifndef`, then exposes them as `config::` constexprs.
  Add new settings the same way, and add a `static_assert` in `main.cpp` if a bad value could
  put a bad signal on the air.
- `include/config_local.h` (user overrides) and `include/secrets.h` (WiFi, `WEB_PASSWORD`,
  `OTA_PASSWORD`) are **gitignored**. So is `platformio_override.ini`. Never commit them or copy their
  values into tracked files. Keep the matching `*.example` files up to date when you add options.
- `CFG_CALLSIGN` / `CFG_LOCATOR` have no default on purpose. Don't add one.

## Invariants to preserve

- **Timing.** Symbol timing is computed with integer maths from the UTC slot start
  (`symbolAt` / `symbolStartUs`), not by accumulating delays. Frequencies are in 0.01 Hz (`CENTI_HZ`).
  Don't introduce floating-point drift into either.
- **Concurrency.** `wspr::Beacon` runs in its own task, above `loop()`'s priority. All its public
  methods are thread-safe through a recursive mutex, and `Radio` is also used from both tasks.
  Changes that affect RF go through `Beacon::whenIdle()` (wrapped by `Station::whenIdle`), which
  refuses with `Busy` while on air. Never block or do slow I/O in the beacon task.
- **Station actions** validate input against `limits::`, refuse while transmitting, then `persist()` to
  flash. New controls belong in `Station` and then get wired into both `console.cpp` and `web_ui.cpp`
  (`/api/cmd`), and are documented in the README tables.
- **Safety.** The beacon must stay off the air when it isn't sure: no transmission without a valid
  message, a found Si5351, or NTP synced within `TimeSync::MAX_SYNC_AGE_S`. OTA stays off without a
  password and suspends the station before flashing.
- **Web security.** Keep the Host-header check (DNS rebinding), the `X-Requested-With: wspr` requirement
  on `POST /api/cmd` (CSRF), and optional basic auth.
- **Stay inside the WSPR window.** The random offset is capped at `MAX_RANDOM_OFFSET_HZ` (95 Hz), so
  all four tones stay inside the 200 Hz window.

## Hardware notes

- Target is the ESP32-C3 "Super Mini" / DevKitM-1. Serial goes over native USB CDC (`ARDUINO_USB_CDC_ON_BOOT`).
- The C3 Super Mini needs WiFi TX power reduced (`WiFi.setTxPower(WIFI_POWER_8_5dBm)` in
  `wifi_link.cpp`, commit 516862a). Keep that line.
- I²C defaults are **SDA = GPIO21, SCL = GPIO20**, which matches the owner's board. Keep `config.h`,
  the README wiring table and `config_local.h.example` in agreement, and don't swap them back.

## Style

- C++ with Google-ish formatting: 2-space indent, lines up to about 110 columns, `camelCase` methods,
  `trailingUnderscore_` members, `UPPER_CASE` constants, `enum class`.
- Avoid heap allocation and `String` in hot or periodic paths: use fixed buffers (see `utc_text.h`, `JsonWriter`).
- Comments are short and explain *why*. Each header opens with a comment saying what the module is for.
