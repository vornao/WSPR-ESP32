# WSPR beacon for ESP32-C3 + Si5351

A standalone WSPR beacon. An ESP32-C3 gets UTC from NTP over WiFi, encodes a WSPR
message and keys the 162 channel symbols onto an Si5351 clock generator, starting
1 second after an even UTC minute. It can be driven from a serial console or from a
small web page on your network.

> **You need an amateur radio licence to transmit.** The Si5351 puts out a square
> wave, rich in harmonics — **always run it through a band-appropriate low-pass
> filter**, and test into a dummy load before connecting an antenna. You are
> responsible for staying inside your licence conditions and your national band plan.

## Features

- WSPR Type 1 messages (callsign, 4-char locator, power), and with a 6-char locator
  alternating Type 1 / Type 3 so receivers can resolve your full subsquare
- Timing anchored to the UTC slot start, resynced from NTP
- Transmits every N-th 2-minute slot, with a random ±80 Hz offset inside the 200 Hz window
- Serial console: frequency, calibration, drive strength, test carrier, tone sweep, TX log
- Web interface at `http://wspr.local/` with a JSON API, optional HTTP basic auth
- Firmware updates over WiFi (ArduinoOTA), which pause the beacon and get off the air first
- Runtime changes saved to flash (NVS) and restored on boot

## Hardware

| | |
|---|---|
| MCU | ESP32-C3 (DevKitM-1, "Super Mini" or most clones) |
| Synthesizer | Si5351A breakout, I²C address `0x60`, output on CLK0 |
| Filter | Low-pass filter for your band — required |

Default wiring (change the pins in `config_local.h` if yours differs):

| ESP32-C3 | Si5351 |
|---|---|
| GPIO20 | SDA |
| GPIO21 | SCL |
| 3V3 | VIN |
| GND | GND |

`CLK0` → low-pass filter → dummy load or antenna. The onboard LED (GPIO8,
active-low) is solid while transmitting and blinks while the test carrier is on.

## Setup

```sh
git clone <this repo>
cd WSPR-ESP32

cp include/secrets.h.example       include/secrets.h
cp include/config_local.h.example  include/config_local.h
```

Edit `include/secrets.h` with your WiFi credentials, and `include/config_local.h`
with at least your callsign and locator. The build fails with a clear message until
those two are set — transmitting with someone else's callsign is illegal, so there is
deliberately no default.

Then build and flash over USB:

```sh
pio run -t upload
pio device monitor
```

## Configuration

Settings are split into three files so that your station never ends up in git:

| File | Tracked | Contents |
|---|---|---|
| `include/config.h` | yes | every setting, with documented defaults — **don't edit this** |
| `include/config_local.h` | **no** | your overrides: callsign, locator, calibration, pins |
| `include/secrets.h` | **no** | WiFi credentials, web and OTA passwords |

`config_local.h` only needs the settings you actually want to change; anything you
leave out keeps its default from `config.h`. Because you never edit the tracked file,
`git pull` will not conflict with your station. Run
`cat include/config.h` for the full list of `CFG_*` settings — band, duty cycle,
crystal frequency, drive strength, mDNS name and so on.

Changes made at runtime from the console or the web page (frequency, correction,
drive, interval, message mode, beacon on/off) are saved to flash and survive a
restart. They take precedence over `config.h` on the next boot; `r yes` on the
console erases the saved copy and goes back to the file defaults.

### Calibration

`CFG_CORRECTION_PPB` corrects your Si5351 crystal's error in parts per billion. It is
specific to your board and **the default of 0 will leave you off frequency**, likely
outside the 200 Hz WSPR window. To measure it:

1. Enable the test carrier with `o` on the serial console.
2. Measure the actual output frequency with a calibrated receiver, SDR or counter.
3. Correction in ppb ≈ `(nominal − measured) / nominal × 1e9`.
4. Try it live with `c <ppb>`, then `+` / `-` to check; when you are happy, put the
   value in `config_local.h` so it survives a settings reset.

## Serial console

115200 baud. `h` prints this menu:

| | |
|---|---|
| `b` | beacon on/off |
| `n` | transmit in the next slot |
| `x` | abort current transmission |
| `i <N>` | transmit every N 2-minute slots |
| `m <0\|1\|2>` | message: 0 alternate T1/T3, 1 Type 1 only, 2 Type 3 only |
| `f <Hz>` | set centre frequency |
| `+` / `-` | step frequency up / down |
| `s <Hz>` | set step size |
| `c <ppb>` | set crystal correction |
| `d <2\|4\|6\|8>` | drive strength in mA |
| `o` | test carrier on/off |
| `t` | tone test: the 4 WSPR tones, 2 s each |
| `p` | print state |
| `l` | transmission log (last 20) |
| `r yes` | restore `config.h` defaults |

## Web interface

`http://wspr.local/` (or the IP printed on the serial console) mirrors the console:
live state, the TX log, and the same controls.

| Endpoint | |
|---|---|
| `GET /` | control page |
| `GET /api/state` | current state as JSON |
| `GET /api/history` | transmission log as JSON |
| `POST /api/cmd?cmd=<name>&value=<n>` | `next`, `cancel`, `reset`, `freq`, `correction`, `drive`, `carrier`, `beacon`, `interval`, `msgmode` |

**The web interface has no login unless you set one.** Without `WEB_PASSWORD`, anyone
who can reach the board on your network can key the transmitter. Set it in
`include/secrets.h` to require HTTP basic auth as user `admin`. Note that basic auth
over plain HTTP sends the password in reversible form on every request — it keeps
housemates out, it is not a defence on a hostile network.

## Firmware updates over WiFi

```sh
pio run -e esp32c3-ota -t upload
```

The board must be running and on the same network. An update aborts any transmission
and disables the output before flashing, and restores the beacon if the update fails.

**Set `OTA_PASSWORD` in `include/secrets.h`.** Without it, anyone on your network can
flash arbitrary firmware onto a device wired to a transmitter. The uploader needs the
same password — put it in `platformio_override.ini`, which is gitignored:

```sh
cp platformio_override.ini.example platformio_override.ini
```

Never put it in `platformio.ini`; that file is tracked by git.

## Source layout

| | |
|---|---|
| `radio` | Si5351 on CLK0: frequency, output, drive, correction |
| `wspr` | message encoding, tone and symbol timing |
| `time_sync` | WiFi + NTP, UTC clock |
| `beacon` | slot scheduling and symbol keying |
| `status_led` | LED states |
| `station` | control actions shared by the console and the web interface |
| `console` | serial command menu |
| `web_ui` | web page and JSON API |
| `settings` | runtime settings in flash |
| `ota` | firmware updates over WiFi |

## Dependencies

Pulled in automatically by PlatformIO:

- [Etherkit Si5351](https://github.com/etherkit/Si5351Arduino)
- [Etherkit JTEncode](https://github.com/etherkit/JTEncode)

## Licence

MIT — see [LICENSE](LICENSE).
