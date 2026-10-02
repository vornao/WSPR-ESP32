# Changelog

Notable changes to the firmware.

## Unreleased

### Fixed

- **Web page hanging on connect.** The web server handles one connection at a time and
  waited up to 5 s for a request on each. Browsers open spare connections they may never use,
  and one of those could hold up every other request, including the page itself. An idle
  connection is now dropped after 500 ms as soon as another one is waiting.
- **Polling pile-up.** The page asked for `/api/state` every second whether or not the last
  reply had arrived, so a slow moment queued more and more requests in the browser. It now
  polls only after the previous reply, every request has a time limit (4 s, 8 s for commands),
  and hidden tabs stop polling.
- **Slow responses and unreliable `wspr.local`.** WiFi modem sleep is now off: it delayed
  incoming packets and missed mDNS queries. This raises the board's idle current draw.
- **mDNS never recovering.** If mDNS failed to start it was never tried again; it now retries
  every 30 s until it starts.
- The page no longer triggers a `/favicon.ico` request, one less connection per load.
- The Host-header check no longer allocates strings on every request.

### Changed

- WiFi moved out of `time_sync` into a new `wifi_link` module. `setup()` starts it, and the
  modules that need the network (web interface, console, OTA) get it from `main.cpp`.
- `TimeSync` is NTP only, and `loop()` starts it once WiFi is connected. Another clock
  source (e.g. GPS) can replace it without touching the WiFi code.
- After a command without a reply the page now says "no reply from the device; check the
  state before retrying", since the command may still have been applied.
