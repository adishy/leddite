# 0015 — OTA by upload to the device, in a window opened at the panel

- **Date:** 2026-08-30
- **Status:** Accepted — supersedes the transport half of [0014](0014-ota-by-pull-with-deferred-rollback.md)
- **Context branch:** `v2-games-overhaul`

## Context

[0014](0014-ota-by-pull-with-deferred-rollback.md) chose a **pull**: the device
fetches an image from a URL baked in at compile time when somebody selects
`Settings → UPDATE → YES`. It was the cheapest of the three options measured
(+13,936 B against +31,400 for an upload form and +54,260 for `ArduinoOTA`) and
the only one that left nothing listening.

It never completed once on real hardware.

The device cannot open a TCP connection to the build host. Measured from the
panel itself, repeatedly, over two days:

| Target | Result |
|---|---|
| `192.168.0.1:80` (gateway) | connected, 8 ms |
| `192.168.0.1:443` | connected, 21 ms |
| `1.1.1.1:80` (internet) | connected, 20 ms |
| `192.168.0.185:8123` (image server) | **timeout at 4002 ms** |
| `192.168.0.185:22` | **timeout at 4002 ms** |

Both hosts are `192.168.0.x/24` on the same SSID. `ping` from the host to the
device succeeds at **ttl=63** while the gateway answers at **ttl=64** — one
router hop. The panel is behind a repeater, believes the host is on-link, ARPs
for it directly, and that ARP never crosses the router. No firewall rule fixes
this; a `ufw allow` was added on a wrong diagnosis and made no difference,
because nothing was ever arriving to be dropped.

Three separate problems, only one of which is the network:

1. **It needs a server.** Somebody must run one and keep it running.
2. **It needs the device to reach you** — the one direction that does not work
   here, and the one the firmware has no say over.
3. **The URL is compile-time.** Changing where updates come from needs the cable
   the feature exists to avoid.

## Decision

**The browser uploads the image to the device.** `Settings → UPDATE → YES` opens
an HTTP server on port 80 for five minutes and scrolls the device's own URL
across the panel. You browse to it, drop a `.bin` on the page, and it is written
straight into the spare app slot.

This inverts all three problems. The browser already holds the file; the
connection runs browser → device, which is the direction that works here and has
always worked (the WebSocket canvas on `:81` depends on it); and there is nothing
to configure, because **the address you need is on the panel in front of you**.

### On "nothing listens", which 0014 was right about

0014's deciding factor was that the pull leaves no open socket. That argument was
against a server running for the device's **entire uptime**, accepting firmware
from anyone on the LAN — which is what `ArduinoOTA` and a permanent upload form
both do.

The window here is not that. It exists only after a person turns a knob to YES
and presses it, closes on the next press, on a long-press, on completion, and on
a five-minute timeout (`UiController::OTA_WINDOW_MS`, enforced in
`UiController::update()` and tested). Physical presence remains the
authentication factor. The property 0014 cared about survives; the cost it
avoided — 17 KB — turned out to buy a feature that works.

### Cost

Measured against this firmware, `min_spiffs`, esp32 core 3.3.8:

| | Flash | RAM (static) |
|---|---|---|
| 0014's pull | 1,207,868 (61%) | 72,164 (22%) |
| This upload flow | 1,231,532 (62%) | 72,012 (21%) |
| **Delta** | **+23,664** | **&minus;152** |

Cheaper than 0014's +31,400 estimate for an upload form, because `Update` was
already linked and `HTTPUpdate` goes away. Static RAM goes *down*: the
`WebServer` is `new`ed when the window opens and deleted when it closes, so it
costs nothing for the 99.9% of uptime the panel is not being updated. The
`WiFiClientSecure`/`HTTPClient` pair stays regardless — `WeatherClient` needs it.

### The URL is shown whole

The panel scrolls `HTTP://192.168.0.113`, not `192.168.0.113`. A bare dotted quad
leaves the reader guessing at the scheme and the port, and this is a string being
copied by hand onto another device. The server therefore binds **port 80**, so
there is no `:8123` suffix to read off a 16-pixel display and mistype. This cost
one new glyph — `/` — in the 5×7 font, which had it as a blank.

### What the split with `UiController` bought

Nothing in `src/` knows what HTTP is, and that did not change. `UiController`
gained one phase (`WAITING`) and a timeout; the transport swap underneath it was
invisible to the screens, the tests and the WASM harness. That is the payoff from
[0009](0009-mode-logic-in-src-not-firmware.md) landing on a concrete bill: a complete
change of update mechanism touched the state machine by one enum value.

### Rollback is unchanged

The deferred-rollback machinery from 0014 — overriding the core's weak
`verifyRollbackLater()` so a new image is only marked valid after
`HEALTHY_AFTER_MS` of connected uptime — is transport-independent and carries over
untouched. It remains the highest-value part of the OTA work.

## Consequences

**Good**

- The update path works on the network the device is actually on.
- No image server, no `ota_config.h`, no generator script, no firewall rule.
  `tools/gen-ota-config.sh` and `esp32_firmware/ota_config.h.example` are deleted.
- Updating from a phone works, because it is just a web page.
- The version to flash is chosen at upload time, not at compile time.

**Costs**

- **An open HTTP server, briefly.** Anyone on the LAN who reaches the device
  during the five-minute window can upload firmware. Mitigated by the window
  being short and physically opened, not by authentication. If that ever matters,
  the lever is signed images (`Update/src/Updater_Signing.{h,cpp}` behind
  `UPDATE_SIGN`) rather than a password on the page — the same conclusion 0014
  reached about the fetch.
- **Plain HTTP, no TLS.** A device-hosted certificate would have to be either
  self-signed (a browser warning on every update) or impossible (no public
  hostname). The upload crosses one LAN hop to a device you are standing next to.
- The page is served from flash as a string literal, so changing it is a firmware
  change. It is deliberately dependency-free — a page that fetched a CDN would
  fail on exactly the isolated network where this flow is most useful.
- `ERR` still cannot say why. The browser now can, which is a real improvement:
  it prints the device's own error string.

## Related

- **0014** — the pull this replaces, and the rollback design it keeps.
- **0009** — why `UiController` is Arduino-free, which is what made the transport
  swap cheap.
- **0011** — words, not icons: every OTA state is still a word.
- **0012** — the progress track is drawn by coverage, not brightness, so it does
  not vanish at brightness level 1.
