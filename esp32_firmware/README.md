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

From the repo root:

```bash
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" \
  compile -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino
```

---

## Flash

Find the ESP32 port (usually `/dev/cu.usbserial-0001` on macOS):

```bash
ls /dev/cu.usbserial*
```

Flash:

```bash
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" \
  upload -p /dev/cu.usbserial-0001 -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino
```

---

## Startup Behaviour

On boot the firmware:
1. Initialises FastLED (panel goes dark).
2. Connects to WiFi — prints dots to serial at 115200 baud.
3. Syncs NTP time (Eastern Time, auto-DST).
4. **Flashes the entire display solid green for 500 ms** once ready.
5. Shows the **boot menu** on the LED panel — rotate encoder to select:
   - `CK` — Clock + Calendar (NTP time / scrolling date)
   - `NT` — Network Canvas (WebSocket binary protocol, port 81)
   - `PT` — Pattern Slideshow (rainbow, lava lamp, pulse, sparkle)
   - `TM` — Visual Timer (rotary encoder sets minutes)
6. Press encoder to enter selected mode. Press again to return to menu.
   In Network Canvas mode: long-press (2s) to return to menu.

Serial output at 115200 baud shows IP, mode transitions, and encoder events.

### Rotary Encoder
- **CLK** → GPIO 32  |  **DT** → GPIO 33  |  **Button** → GPIO 25
- Turn: navigate menu / adjust timer / skip pattern
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
