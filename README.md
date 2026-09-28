# WSPR beacon for ESP32-C3 + Si5351

A standalone [WSPR](https://www.wsprnet.org/) beacon. An ESP32-C3 gets UTC from NTP over
WiFi, encodes your callsign, locator and power, and keys the 162 WSPR symbols onto an
Si5351 clock generator at the start of each chosen 2-minute slot. You control it from a
serial console or from a web page on your network, and the web page also shows who heard you.

The WSPR engine lives in [`lib/wspr`](lib/wspr/src/wspr.h), separate from the rest of the
firmware, so you can reuse it with another synthesizer or time source.

> [!WARNING]
> **You need an amateur radio licence to transmit.** The Si5351 outputs a square wave
> full of harmonics, so **always put a low-pass filter for your band after it**, and test
> into a dummy load before you connect an antenna. Staying within your licence and your
> national band plan is your responsibility.

## Features

- **WSPR Type 1** messages (callsign, 4-character locator, power). With a 6-character
  locator it alternates **Type 1 / Type 3**, so receivers can resolve your full subsquare.
- **Accurate timing.** The beacon runs in its own FreeRTOS task, and each symbol's timing
  is computed from the UTC slot start. Web traffic, flash writes and serial output can't
  delay it. If NTP hasn't synced for 12 hours, the beacon stops transmitting until it does.
- **Scheduling.** It transmits every N-th 2-minute slot, each time at a random offset of
  up to ±80 Hz, always inside the 200 Hz WSPR window. You can also send in the next slot
  on demand.
- **Serial console.** Set the frequency, calibration and drive strength, run a test
  carrier or tone sweep, and read the TX log.
- **Web interface** at `http://wspr.local/`, with a JSON API and a list of spots from
  [wspr.live](https://wspr.live/). It is protected against CSRF and DNS rebinding, and a
  login is optional.
- **Firmware updates over WiFi.** They need a password, and the board goes off the air
  before flashing.
- **Settings stay in flash.** Changes you make at runtime survive a restart.
- **Checked at build time.** An invalid power, frequency, interval or offset fails the build.

## Hardware

| Part | Notes |
|---|---|
| MCU | ESP32-C3: DevKitM-1, "Super Mini" or most clones |
| Synthesizer | Si5351A breakout, I²C address `0x60`, output on CLK0 |
| Filter | Low-pass filter for your band. **Required.** |

Default wiring (you can change the pins in `config_local.h`):

| ESP32-C3 | Si5351 |
|---|---|
| GPIO20 | SDA |
| GPIO21 | SCL |
| 3V3 | VIN |
| GND | GND |

`CLK0` → low-pass filter → dummy load or antenna. The onboard LED (GPIO8, active-low)
stays on while transmitting and blinks while a test output is on.

## Quick start

You need [PlatformIO](https://platformio.org/) (the CLI or the VS Code extension).

```sh
git clone <this repo> && cd WSPR-ESP32

cp include/secrets.h.example       include/secrets.h        # WiFi, passwords
cp include/config_local.h.example  include/config_local.h   # your station
```

1. Put your WiFi credentials in `include/secrets.h`.
2. Set `CFG_CALLSIGN` and `CFG_LOCATOR` in `include/config_local.h`. There are no
   defaults, because transmitting under someone else's callsign is illegal, so the build
   stops until you set them.
3. Build, flash and open the console:

   ```sh
   pio run -t upload
   pio device monitor
   ```

4. [Calibrate](#calibration) your board before you connect an antenna.

## Configuration

Settings are split across three files, so your station details never end up in git:

| File | In git | Contents |
|---|---|---|
| `include/config.h` | yes | every setting, with its documented default. **Don't edit this file.** |
| `include/config_local.h` | no | your overrides: callsign, locator, calibration, pins |
| `include/secrets.h` | no | WiFi credentials, web and OTA passwords |

In `config_local.h`, define only what you want to change. Everything else keeps its
default, and `git pull` never conflicts with your settings.

| Setting | Default | |
|---|---|---|
| `CFG_CALLSIGN` | — | **required**; 3–6 characters (compound calls with `/` aren't supported) |
| `CFG_LOCATOR` | — | **required**; 4 or 6 character Maidenhead locator |
| `CFG_POWER_DBM` | `20` | reported power: 0–60, ending in 0, 3 or 7 (20 = 100 mW, 30 = 1 W) |
| `CFG_CORRECTION_PPB` | `0` | crystal error, see [Calibration](#calibration) |
| `CFG_CENTER_FREQ_HZ` | `14097100` | centre of the 200 Hz WSPR window (see below) |
| `CFG_TX_EVERY_N_SLOTS` | `3` | send once every N 2-minute slots, 1–30 |
| `CFG_TX_RANDOM_OFFSET_HZ` | `80` | random ± offset per transmission, 0–95 |
| `CFG_XTAL_FREQ_HZ` | `25000000` | some Si5351 boards use 27 MHz |
| `CFG_XTAL_LOAD_PF` | `8` | 6, 8 or 10 |
| `CFG_DRIVE_MA` | `8` | output drive: 2, 4, 6 or 8 mA |
| `CFG_SDA_PIN` / `CFG_SCL_PIN` | `20` / `21` | I²C pins |
| `CFG_LED_PIN` / `CFG_LED_ACTIVE_LOW` | `8` / `true` | status LED |
| `CFG_MDNS_NAME` | `"wspr"` | web interface at `http://<name>.local/` |
| `CFG_WEB_CHECK_HOST` | `true` | see [Security](#security) |
| `CFG_NTP_SERVER` | `"pool.ntp.org"` | |

Centre frequencies by band: 40 m `7040100`, 30 m `10140200`, 20 m `14097100`,
17 m `18106100`, 15 m `21096100`. Use a low-pass filter to match.

When you change the frequency, correction, drive, interval, message mode or beacon on/off
from the console or the web page, the new value is saved to flash. Saved values take
precedence over `config.h` at boot. `r yes` on the console, or *Restore defaults* on the
web page, erases the saved copy.

### Calibration

`CFG_CORRECTION_PPB` corrects the error of your Si5351's crystal. Every board is
different, and **with the default of 0 you will probably be outside the 200 Hz WSPR
window.** To measure yours:

1. Turn on the test carrier with `o` on the console (or the switch on the web page).
2. Measure the output frequency with a calibrated receiver, SDR or frequency counter.
3. Work out the correction in ppb: `(nominal − measured) / nominal × 1e9`.
4. Try it live with `c <ppb>` and check again. Once it's right, put the value in
   `config_local.h` so that a settings reset keeps it.

The tone sweep (`t`) steps through the four WSPR tones, 1.46 Hz apart, so you can check
the fine frequency steps on a waterfall.

## Serial console

115200 baud. Type `h` for this list:

| Command | |
|---|---|
| `b` | beacon on/off |
| `n` | transmit in the next slot |
| `x` | abort the current transmission |
| `i <N>` | transmit every N 2-minute slots |
| `m <0\|1\|2>` | message: 0 alternate T1/T3, 1 Type 1 only, 2 Type 3 only |
| `f <Hz>` | set the centre frequency |
| `+` / `-` | step the frequency up / down (no Enter needed) |
| `s <Hz>` | set the step size |
| `c <ppb>` | set the crystal correction |
| `d <2\|4\|6\|8>` | drive strength in mA |
| `o` | test carrier on/off |
| `t` | tone sweep; any key stops it |
| `p` | print the state |
| `l` | transmission log (last 20) |
| `r yes` | restore the `config.h` defaults |

Transmissions are logged as they start and end.

## Web interface

Open `http://wspr.local/`, or the IP address the console prints. The page shows the live
state, a countdown to the next transmission, the TX log and recent spots, and has the
same controls as the console. Your browser fetches the spots straight from wspr.live, so
they only appear if the browser has internet access.

| Endpoint | |
|---|---|
| `GET /` | control page |
| `GET /api/state` | current state as JSON |
| `GET /api/history` | transmission log as JSON |
| `POST /api/cmd` | form fields `cmd=<name>&value=<n>`. Names: `next`, `cancel`, `reset`, `freq`, `correction`, `drive`, `carrier`, `tones`, `beacon`, `interval`, `msgmode` |

`POST /api/cmd` requires the header `X-Requested-With: wspr`:

```sh
curl -X POST -H 'X-Requested-With: wspr' -d 'cmd=next' http://wspr.local/api/cmd
```

## Security

The board controls a transmitter, so it refuses anything it can't trust:

- **Web login (optional).** Set `WEB_PASSWORD` in `secrets.h` to require HTTP basic
  auth as user `admin`. Without it, anyone on your network can use the controls. Basic
  auth over plain HTTP keeps housemates out, but it won't protect you on a network you
  don't trust.
- **Cross-site requests are refused.** Commands need a custom header that a web page on
  another site can't send. So a malicious page you open can't key your transmitter,
  even if you're logged in.
- **DNS rebinding is refused.** The board only answers requests addressed to
  `<name>`, `<name>.local` or its IP address. If you reach it through a reverse proxy
  or another DNS name, set `CFG_WEB_CHECK_HOST` to `false`.
- **OTA needs a password.** Updates over WiFi are **off** unless `OTA_PASSWORD` is set,
  so nobody else can flash firmware onto the board.
- **Secrets stay out of git.** `secrets.h`, `config_local.h` and
  `platformio_override.ini` are gitignored.

## Firmware updates over WiFi

1. Set `OTA_PASSWORD` in `include/secrets.h` and flash once over USB.
2. Give the uploader the same password in `platformio_override.ini`, which is gitignored
   (never put it in `platformio.ini`, which is tracked):

   ```sh
   cp platformio_override.ini.example platformio_override.ini   # then edit
   ```

3. From then on:

   ```sh
   pio run -e esp32c3-ota -t upload
   ```

When an update starts, the board aborts any transmission and switches its output off.
If the update fails, it goes back to beaconing.

## Project layout

```
lib/wspr/              reusable WSPR engine
  wspr_protocol.*        timing, tone frequencies, field validation (portable C++)
  wspr_schedule.*        which slots to transmit in (portable C++)
  wspr_message.*         message encoding (Etherkit JTEncode)
  wspr_beacon.*          beacon engine: FreeRTOS task keying a Transmitter (ESP32)
src/                   this firmware
  main.cpp               wiring
  radio.*                Si5351 on CLK0, as a wspr::Transmitter
  time_sync.*            WiFi + NTP, as a wspr::Clock
  station.*              control actions shared by the console and the web UI
  console.*              serial menu
  web_ui.*, web_page.h   web page and JSON API
  settings.*             runtime settings in flash
  ota.*                  updates over WiFi
test/test_wspr/        host unit tests for lib/wspr
```

### Reusing the beacon engine

`wspr::Beacon` works with any RF source and any clock that implements these two
interfaces:

```cpp
#include <wspr.h>

class MySynth : public wspr::Transmitter {
 public:
  bool ready() const override;
  void setFrequencyCentiHz(uint64_t centiHz) override;  // 0.01 Hz units
  void setOutput(bool on) override;
};

class MyClock : public wspr::Clock {
 public:
  bool synced() const override;   // time good to well under a second?
  int64_t utcUs() const override; // microseconds since the Unix epoch
};

MySynth synth;
MyClock utc;
wspr::Message message;
wspr::Beacon beacon(synth, message, utc);

void setup() {
  message.encode("K1ABC", "FN42", 23);
  beacon.setCenterHz(14097100);
  beacon.setEveryNSlots(5);
  beacon.setRandomOffsetHz(80);
  beacon.begin();  // starts the beacon task
}
```

Every `Beacon` method is thread-safe. `state()` returns a snapshot for display, and
`whenIdle()` runs a change only when no transmission is on air.

## Tests

The portable part of `lib/wspr` (symbol timing, tone frequencies, callsign/locator/power
validation and slot scheduling) is unit-tested on your computer:

```sh
pio test -e native
```

## Troubleshooting

| Symptom | Check |
|---|---|
| `Si5351 not found at 0x60` | SDA/SCL wiring, 3V3 power, pull-ups on the breakout |
| `can't send callsign ... in WSPR` | callsign is 3–6 characters with a digit in the right place; locator is 4 or 6 characters |
| Nobody decodes you | calibration (`o`, then measure), low-pass filter, antenna. Also check `p` shows `time: ... UTC` |
| Web page unreachable by name | your network may block mDNS; use the IP address the console prints |
| `403 unknown host name` | you are using a different hostname or a proxy; set `CFG_WEB_CHECK_HOST false` |
| OTA upload refused | `OTA_PASSWORD` set in `secrets.h` and the same `--auth` in `platformio_override.ini` |

## Dependencies

PlatformIO installs these automatically:

- [Etherkit Si5351](https://github.com/etherkit/Si5351Arduino)
- [Etherkit JTEncode](https://github.com/etherkit/JTEncode)

## Licence

MIT. See [LICENSE](LICENSE).
