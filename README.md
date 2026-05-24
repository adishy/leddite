# Leddite V2

16×16 WS2812B LED matrix driven by an ESP32, with a rotary encoder boot menu,
four built-in modes, a WASM-powered browser simulator, and a binary WebSocket API
for programmatic control.

---

## Hardware

| Component | Detail |
|-----------|--------|
| LED panel | 256× WS2812B (16×16), GPIO 4, GRB order |
| MCU | ESP32 (240 MHz dual-core) |
| Encoder | CLK→GPIO32, DT→GPIO33, Button→GPIO25 |
| Enclosure | Laser-cut acrylic + plywood — files in `hardware_build/` |

---

## Boot menu

On power-up, the display shows a boot menu.  Rotate the encoder to navigate,
press to select.

| Mode | Label | Description |
|------|-------|-------------|
| Clock + Calendar | **CK** | 24-hour clock (HH sky-blue / MM pink); DVD-bounces around screen once/sec; alternates with date (DD orange / MMM green) every 10 s |
| Network Canvas | **NT** | WebSocket binary API (port 81); encoder events broadcast as JSON |
| Pattern Slideshow | **PT** | Rainbow wave, lava lamp, pulse, sparkle; auto-advance 15 s |
| Visual Timer | **TM** | Encoder sets minutes (1–90), progress-bar countdown |
| Characters | **OC** | Animated Pac-Man ghost; press cycles 5 colour palettes |

**Universal gestures**
- Long press (3 s) → back to boot menu from any mode
- Long press *in* menu → screen off; short press wakes

---

## Quick start — simulator

**Prerequisites:** Python 3.10+, [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (for rebuilding WASM)

```bash
# Install Python dependencies
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt

# (Optional) rebuild WASM from C++ source — pre-built WASM is committed
make simulator

# Start simulator server + HTTP file server
.venv/bin/python simulator_server.py

# Open the simulator in your browser
open http://localhost:8000

# Run the test suite against the simulator (new terminal)
.venv/bin/python test_suite.py
```

The simulator browser tab renders the 16×16 grid with WASM logic identical to
the ESP32.  It also exposes an **encoder UI** (↺ CCW / ⏺ Press / ↻ CW buttons
and ← → Enter keyboard shortcuts) that sends JSON encoder events to any connected
test clients.

---

## Quick start — hardware

See [`esp32_firmware/README.md`](esp32_firmware/README.md) for the complete
guide: prerequisites, WiFi credentials, compile, flash, and serial monitoring.

**TL;DR** (macOS, Arduino IDE 2.x installed):

```bash
# Compile
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" \
  compile --fqbn esp32:esp32:esp32 esp32_firmware/

# Flash (adjust port)
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" \
  upload --fqbn esp32:esp32:esp32 --port /dev/cu.usbserial-0001 esp32_firmware/

# Run test suite against hardware
.venv/bin/python test_suite.py 192.168.1.100 81
```

---

## Python API

```python
from leddite_client import LedditeClient

client = LedditeClient("192.168.1.100", 81)  # or ("localhost", 8765) for simulator
await client.connect()

await client.clear()
await client.write_text("HI", x=2, y=4, color=(0, 200, 255))
await client.draw_rect(0, 0, 16, 16, color=(10, 0, 30))

# Listen for encoder events (Network Canvas mode)
async def on_encoder(ev):
    print(ev)  # {"type": "encoder", "delta": 1}

await client.listen_encoder(on_encoder)
```

---

## LLM scenes

Requires a Gemini API key in `.env.secrets`:

```
LLM_API_KEY=your_key_here
```

```bash
.venv/bin/python llm_scenes.py "a cyberpunk cityscape at night"
```

---

## Development

```bash
# Native C++ unit tests (31 tests — no Arduino needed)
make test

# Full e2e via Claude Code skill
/e2e-test            # simulator
/e2e-test 192.168.1.100   # hardware

# Environment setup / verification
/work-setup
```

### Project layout

```
esp32_firmware/       Production ESP32 sketch + all mode modules
  AppState.h            AppMode enum (MENU, CLOCK_CAL, NETWORK, PATTERN, TIMER, OCTOPUS, OFF)
  EncoderInput.*        Rotary encoder driver (CLK/DT/button, long-press detection)
  MenuMode.*            Boot menu (scrolling name, indicator dots, warm color palette)
  TimeMode.*            Clock + Calendar mode
  PatternMode.*         Pattern slideshow (4 patterns, auto-advance)
  TimerMode.*           Visual countdown timer
  NetworkMode.*         WebSocket binary protocol + encoder JSON broadcast
  Canvas/Transformer/MarqueeEngine/ProtocolHandler/TextRenderer — mirrors of src/

src/, include/        Native-compilable C++ core (no Arduino deps)
test/                 C++ unit tests (make test)
simulator/            WASM-powered browser simulator
  index.html            Grid display + encoder UI buttons + keyboard shortcuts
  simulator.js          WASM bindings, WS relay, encoder event handling
  wasm_bridge.cpp       Emscripten bindings for Canvas/Transformer/MarqueeEngine
simulator_server.py   Combined HTTP (8000) + WebSocket relay (8765) server
test_suite.py         E2E tests: shapes, text, marquee, 60fps, encoder events,
                        visual verify walkthrough (--verify flag)
leddite_client.py     Python WebSocket client (binary sprite API + encoder listener)
llm_scenes.py         LLM-powered scene generator (Gemini)
dsl_runner.py         Simple DSL interpreter for scripted scenes
docs/                 Obsidian documentation vault
hardware_build/       CAD files (SVG/PDF) for laser-cut enclosure
hw_setup/             Raspberry Pi service files (legacy)
```

---

## Binary protocol

```
Header (8 bytes):
  [0] version   = 1
  [1] flags     bit 0 = clear canvas, bit 1 = show immediately, bit 2 = marquee
  [2] width     sprite width (pixels)
  [3] height    sprite height (pixels)
  [4] x_offset  int8_t — supports negative (off-screen left)
  [5] y_offset  int8_t — supports negative
  [6] rotation  0–3 (0°, 90°, 180°, 270°)
  [7] brightness 0–255

Payload: width × height × 3 bytes (raw RGB)
```

Encoder events (JSON TEXT frames, Network Canvas mode only):
```json
{"type": "encoder", "delta": 1}           // clockwise turn
{"type": "encoder", "delta": -1}          // counter-clockwise turn
{"type": "encoder", "button": "pressed"}  // short press
{"type": "encoder", "button": "released"} // release
```

---

## Docs

The `docs/` directory is an Obsidian vault with deep-dives on each component:
Canvas, Transformer, MarqueeEngine, Protocol, Simulator, LedditeClient,
HardwareMapping, Modes, EncoderInput.
