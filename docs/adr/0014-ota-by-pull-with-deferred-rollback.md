# 0014 — OTA by pull, triggered at the panel, with deferred rollback

- **Date:** 2026-08-30
- **Status:** Superseded by [0015](0015-ota-by-upload-to-the-device.md) — the
  transport is now a browser upload, because this pull never once completed on
  real hardware (the device cannot open a connection to the build host). The
  deferred-rollback design below is unchanged and still in force.
- **Context branch:** `v2-games-overhaul`

## Context

Updating the panel meant unplugging it and carrying it to a machine with
`arduino-cli`. The obvious blocker was `CLAUDE.md`'s standing warning that flash
sits at ~90%, which reads as "there is no room for a second copy of the image".

That turned out to be a misreading. `default.csv` already ships **two OTA app
slots** — `app0` at 0x10000 and `app1` at 0x150000, 0x140000 each — plus an
`otadata` partition. The 90% was always 90% *of one slot*. OTA needed no
partition change to be possible at all.

## Measurements

Three approaches, built against this firmware on esp32 core 3.3.8. Baseline for
these was 1,184,240 B; the numbers below are the deltas measured at the time.

| Approach | Flash | RAM | Leaves something listening |
|---|---|---|---|
| `ArduinoOTA` (+ mDNS) | +54,260 B | +4,712 B | yes, permanently |
| `WebServer` upload form + `Update` | +31,400 B | +624 B | yes, permanently |
| **`HTTPUpdate` pull** | **+13,936 B** | **+384 B** | **no** |

`ArduinoOTA` references the global `MDNS` object unconditionally, so
`setMdnsEnabled(false)` saves runtime but not flash.

As integrated here the pull cost **+4,160 B** rather than +13,936 — most of
`HTTPUpdate`'s dependencies are already linked, because `WeatherClient` pulls in
`WiFiClientSecure` and `HTTPClient` regardless. TLS was already paid for.

## Decision

**Pull an image over HTTP(S) from a URL, on demand, from the panel's own menu.**

`Settings -> UPDATE` opens a confirmation that always starts on **NO**; the
device fetches only after somebody standing at it turns to YES and presses.

The deciding factor is not the 14 KB — it is that the pull is the only one of the
three that leaves **nothing listening**. The other two accept firmware from
anyone on the LAN for the entire uptime of the device. Physical presence is a
better authentication factor than a password on an open socket, which is why the
HTTP Basic credentials in `ota_config.h` are a guard against a mistake rather
than against an attacker.

### Deferred rollback

The core enables `CONFIG_APP_ROLLBACK_ENABLE`, and `initArduino()` marks a newly
written image valid **before `setup()` runs**. An image that bricks itself on
boot is therefore already committed by the time it fails.

`OtaUpdater.cpp` overrides the core's weak `verifyRollbackLater()` hook to defer
that, and marks the image valid only after the device has joined WiFi and been
running for `HEALTHY_AFTER_MS` (30 s). A bad image reverts to the previous slot
on the next reboot instead of needing a cable. This is the highest-value part of
the change and it costs almost nothing.

The hook is declared in a `.c` file, so the override needs `extern "C"` linkage.
Without it the definition silently does not bind and the deferral never happens.

### The partition scheme moves to `min_spiffs`

Not because OTA requires it — `default` has the slots — but because it is the
larger win independently: each app slot goes from 0x140000 to 0x1E0000, taking
this build from **91% of a slot to 61%**. Nothing in the repo uses SPIFFS or
LittleFS, and `nvs` stays at 0x9000/0x5000 in both schemes, so saved brightness,
place and unit survive the switch. Verified on the device: after reflashing with
the new layout, the settings line still read `brightness=1 place=CAMB unit=C`.

**The first flash after this change must be over USB.** A partition table is not
part of an OTA payload, so a device still running the `default` layout would
write a `min_spiffs` image into a slot at the wrong offset.

### The split between `UiController` and `OtaUpdater`

`UiController` owns the screens, the confirmation, the progress value and the
request flag, and knows nothing about HTTP (docs/adr/0009). `OtaUpdater` owns the
flash write and reports back through `setOtaProgress()` / `setOtaResult()`.

That split is what makes the flow testable: `test_ui_controller` asserts the
confirmation defaults to NO, that a running write cannot be escaped by either
gesture, and that OK and ERR do not render the same pixels — and the WASM harness
runs the same flow through the committed artifact, with the browser standing in
for the firmware. The only untested part is the fetch itself.

## Consequences

**Good**

- Updating no longer needs a cable, after one cable-based migration.
- A bad image rolls itself back rather than bricking the panel.
- 61% flash instead of 91% — the headroom warning in `CLAUDE.md` is retired.
- `WeatherClient` stays off the air during a write. Two concurrent TLS sessions
  cost ~40 KB of heap each, and that was the one heap collision this firmware
  could actually provoke.

**Costs**

- **`setInsecure()`: no server authentication.** A LAN attacker able to MITM the
  fetch could serve arbitrary firmware. Given the trigger is a physical press,
  this is accepted rather than solved. The correct lever if that changes is
  `Update/src/Updater_Signing.{h,cpp}`, which supports signed images behind a
  `UPDATE_SIGN` define — signing the payload, rather than pinning a certificate
  that then has to be rotated.
- A migration step that cannot itself be delivered over the air.
- `ota_config.h` is a second generated, gitignored header after
  `wifi_credentials.h`. It is guarded by `__has_include` so its absence does not
  break a fresh clone's build — the menu entry stays and reports ERR.
- `HTTP_UPDATE_NO_UPDATES` shows as ERR on the panel. There are only two result
  words and "nothing happened" is closer to ERR than to OK; the serial log
  distinguishes them.

## Related

- **0009** — why the screens live in `UiController` and only the flash write is
  in `esp32_firmware/`.
- **0011** — words, not icons: every OTA state is a word, because "is that arrow
  up or down" is the wrong question to be asking while a device reflashes itself.
