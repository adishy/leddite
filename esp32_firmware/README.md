# Leddite V2 — ESP32 Firmware

## Overview

The ESP32 runs one of two firmware modes depending on what's flashed:

| Mode | File | Use |
|------|------|-----|
| **Network server** | `esp32_firmware.ino` (current) | Production: receives binary sprite packets over WebSocket from `test_suite.py`, `leddite_client.py`, or `llm_scenes.py` |
| **Standalone scenes** | archived in git history | Development/testing without WiFi — bakes scenes directly into firmware |

---

## Prerequisites

Arduino IDE v2.x must be installed. It bundles `arduino-cli` at:
```
/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
```

Add a shell alias to save typing (optional):
```bash
alias arduino-cli="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
```

Required Arduino libraries (install once via Arduino IDE Library Manager or `arduino-cli lib install`):
- **FastLED** — LED driving
- **WebSockets** (arduinoWebSockets by Links2004) — WebSocket server
- **ESP32Encoder** (madhephaestus) — rotary encoder driver (`arduino-cli lib install "ESP32Encoder"`)

Board package: **esp32** by Espressif (install via Board Manager).

---

## WiFi Credentials

Create `esp32_firmware/wifi_credentials.h` (already gitignored — never committed):

```cpp
#pragma once
const char* WIFI_SSID     = "your-network-name";
const char* WIFI_PASSWORD = "your-password";
```

---

## Compile

From the repo root. **Sync the duplicated core modules first** — the Arduino
toolchain cannot see `../src`, so those files exist twice and building without
syncing flashes logic no test covered (`RULES.md` §2):

```bash
tools/sync-firmware-copies.sh
```

**`PartitionScheme=min_spiffs` is mandatory.** It gives two 0x1E0000 app slots
instead of two 0x140000, taking this build from 91% of a slot to ~62% and
leaving room for OTA. Omitting it silently builds for the smaller layout.

```bash
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --output-dir build/fw esp32_firmware/esp32_firmware.ino
```

On macOS, Arduino IDE 2.x bundles `arduino-cli` at
`/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`.

---

## Flash over USB

Needed for the first flash, after any partition-scheme change, and whenever the
device has no working WiFi. Otherwise prefer OTA (below).

Find the port — `/dev/ttyUSB0` on Linux, usually `/dev/cu.usbserial-0001` on
macOS:

```bash
arduino-cli board list
arduino-cli upload -p /dev/ttyUSB0 -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --input-dir build/fw esp32_firmware/esp32_firmware.ino
```

On Linux you must be in the `dialout` group (`id -nG | grep dialout`).

---

## Flash over the air

**The device does not fetch anything — you upload to it.** At the panel:
`Settings → UPDATE`, turn to **YES**, press. It scrolls the URL to browse to
(e.g. `HTTP://192.168.0.113`); open that, drop `build/fw/esp32_firmware.ino.bin`
on the page, press Install.

No image server, nothing to configure, no firewall rule — the connection runs
browser → device. The window closes on the next press, on a long-press, on
completion, or after five minutes.

From a script, with nobody at the encoder:

```bash
curl -s http://<ip>/info                                   # {"version":...,"slot":...}
curl -sf -F "f=@build/fw/esp32_firmware.ino.bin" http://<ip>/update    # prints OK
```

That needs the window already open — build once with
`--build-property "compiler.cpp.extra_flags=-DLEDDITE_TEST_OTA_ON_BOOT"` and the
device opens it at the end of `setup()`.

**You have not succeeded until the slot changes and the image is marked valid:**

```
Firmware 2.2.0 on app1 (pending verify)
[OTA] image on app1 marked valid after 30s healthy
```

`(pending verify)` means probation — an image that cannot stay booted and
connected for 30 s reverts to the previous slot by itself. See
[`docs/adr/0015`](../docs/adr/0015-ota-by-upload-to-the-device.md).

---

## Startup Behaviour

On boot the firmware:
1. Initialises FastLED (panel goes dark).
2. Connects to WiFi — prints dots to serial at 115200 baud.
3. Syncs NTP time (Eastern Time, auto-DST).
4. **Flashes the entire display solid green for 500 ms** once ready.
5. Shows the **boot menu** on the LED panel — rotate encoder to select:
   - `CK` — Clock + Calendar (NTP time / scrolling date / weather)
   - `NT` — Network Canvas (WebSocket binary protocol, port 81)
   - `TM` — Timer (rotary encoder sets minutes)
   - `OC` — Octopus (animated character)
   - `GM` — Games (Snake, Invaders, Dino, Pong, Bricks, or cycle all)
   - `ST` — Settings (brightness, weather place, units, IP, UPDATE)
6. Press encoder to enter selected mode. Press again to return to menu.
   In Network Canvas mode: long-press (2s) to return to menu.

Serial output at 115200 baud shows IP, mode transitions, and encoder events.

### Rotary Encoder
- **CLK** → GPIO 32  |  **DT** → GPIO 33  |  **Button** → GPIO 25
- Turn: navigate menu / adjust timer / cycle character style
- Short press: select / confirm / back to menu
- Long press (2s): back to menu (Network Canvas mode only)

### Network Canvas Encoder Events
While in Network Canvas mode, encoder input is broadcast as JSON TEXT frames:
```json
{"type":"encoder","delta":1}          // clockwise
{"type":"encoder","delta":-1}         // counter-clockwise
{"type":"encoder","button":"pressed"} // short press
```
Receive with `leddite_client.py`: `await client.listen_encoder(my_callback)`

---

## Running the Test Suite

Once the ESP32 is connected and you have its IP:

```bash
# Full scene suite (shapes, text, marquee, bouncing ball)
python test_suite.py 192.168.0.128 81

# LLM-generated scenes
python llm_scenes.py "a cyberpunk cityscape"

# Direct Python client
python leddite_client.py
```

`test_suite.py` defaults to `localhost:8765` (the simulator). Pass the ESP32 IP + port 81 to target hardware directly.

---

## Physical Hardware

See [`docs/Components/HardwareMapping.md`](../docs/Components/HardwareMapping.md) for the full physical LED layout and serpentine mapping derivation.

**Quick reference:**
- GPIO 4 → LED data line
- 256 × WS2812B, GRB order
- Panel mounted 90° rotated; serpentine is column-based right→left
- `getPhysicalIndex(x, y)`:
  - x even → `(15-x)*16 + y`
  - x odd  → `(15-x)*16 + (15-y)`

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Display blank after flash | WiFi not connecting | Check `wifi_credentials.h`, ensure 2.4 GHz |
| No green flash on boot | WiFi credentials wrong | Serial monitor will show connection dots timing out |
| Port busy on upload | Serial monitor open | Close it; or `kill $(lsof -t /dev/cu.usbserial-0001)` |
| Shapes/pixels in wrong position | Wrong mapping | Re-run mapping diagnostic firmware |
| Colours wrong (R↔G swap) | Wrong colour order | Should be `GRB` — do not change |
