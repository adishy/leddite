# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

Leddite V2 is a 16×16 WS2812B LED matrix driven by an ESP32, with a binary WebSocket API for programmatic control, a WASM-powered browser simulator, and a Python client library. The codebase has three distinct layers: ESP32 firmware (C++/Arduino), a native-compilable C++ core (`src/` + `include/`), and Python tooling.

## Commands

### Python environment

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

### C++ unit tests (no Arduino/hardware needed)

```bash
make test                        # build + run all 4 test binaries
make test-text-renderer          # single test binary
./test/test_canvas               # run one already-built binary
```

### Simulator (browser-based, WASM)

```bash
.venv/bin/python simulator_server.py   # HTTP :8000 + WS relay :8765
# open http://localhost:8000 in browser
make simulator                         # rebuild WASM (requires Emscripten) + verifies bindings
make check-wasm                        # verify committed WASM has Canvas bindings (no rebuild)
make run-sim                           # rebuild WASM then start server
```

**Must rebuild WASM after any change to `src/`, `include/`, or `simulator/wasm_bridge.cpp`**, then commit the new `simulator/leddite_wasm.js` + `simulator/leddite_wasm.wasm`. Run `make check-wasm` to verify a committed WASM is valid without rebuilding.

### E2E test suite

```bash
.venv/bin/python test_suite.py                      # against simulator (default)
.venv/bin/python test_suite.py 192.168.1.100 81     # against hardware
.venv/bin/python test_suite.py --verify             # adds human-visual pause prompts
```

### LLM scene generation (requires `LLM_API_KEY` in `.env.secrets`)

```bash
.venv/bin/python llm_scenes.py "a cyberpunk cityscape at night"
```

### Claude Code skills

```
/e2e-test                  # run e2e suite against simulator
/e2e-test 192.168.1.100    # run e2e suite against hardware
/work-setup                # verify environment
```

### ESP32 firmware (macOS, Arduino IDE 2.x)

```bash
# Compile
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" \
  compile -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino

# Flash (adjust port)
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" \
  upload -p /dev/cu.usbserial-0001 -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino
```

WiFi credentials go in `esp32_firmware/wifi_credentials.h` (gitignored):
```cpp
#pragma once
const char* WIFI_SSID     = "your-ssid";
const char* WIFI_PASSWORD = "your-password";
```

## Architecture

### The dual C++ problem

The core rendering logic lives in **two places** that must stay in sync:

| Layer | Location | Compiled with |
|-------|----------|---------------|
| ESP32 production | `esp32_firmware/Canvas.cpp`, `Transformer.cpp`, `MarqueeEngine.cpp`, `ProtocolHandler.cpp`, `TextRenderer.cpp` | arduino-cli (Arduino framework) |
| Native / WASM | `src/` (same filenames) + `include/` headers | `g++` (unit tests) / `emcc` (WASM) |

When editing any of these core files, update **both** copies. The `src/` copies have no Arduino dependencies, making them unit-testable and WASM-compilable.

### Binary WebSocket protocol

All drawing happens via 8-byte header + raw RGB payload:

```
[0] version=1  [1] flags  [2] width  [3] height
[4] x_offset   [5] y_offset (both int8_t — support negatives)
[6] rotation (0–3)  [7] brightness (0–255)
Payload: width × height × 3 bytes (raw RGB)
```

Flag bits: `0x01`=clear before draw, `0x02`=show immediately, `0x04`=marquee mode, `0x08`=reply with canvas ACK.

**Canvas ACK (`0x08` flag):** The receiver immediately replies with `[0xCA, 16, 16, r,g,b×256]` — 771 bytes. This lets `test_suite.py` make pixel-level assertions without vision or browser automation, and works on both simulator and real hardware.

### Simulator architecture

`simulator_server.py` runs two servers in the same process:
- **HTTP :8000** — serves `simulator/` static files + `/canvas-state` JSON endpoint
- **WS :8765** — relays messages between all connected clients (browser + test scripts)

The server also maintains a **Python-native canvas mirror** (exact port of `Canvas.cpp` + `Transformer.cpp`) so `/canvas-state` reflects the latest rendered frame even without a browser open.

The browser (`simulator/index.html` + `simulator.js`) renders via the WASM module (`leddite_wasm.js`/`.wasm`), which is compiled from `src/` + `simulator/wasm_bridge.cpp` using Emscripten. A pre-built WASM is committed so Emscripten is only needed when changing C++ core code.

**WASM pitfalls (both have caused a black/non-rendering simulator):**

1. **`wasm_bridge.cpp` must be included in every `emcc` invocation.** Compiling only `src/*.cpp` without `simulator/wasm_bridge.cpp` produces a valid `.wasm` that passes all Emscripten checks but contains no `Canvas`/`drawSprite`/`getBuffer` bindings. The simulator loads it silently, `new Module.Canvas()` throws, `ledditeCanvas` stays `null`, and every incoming WebSocket frame is dropped in `handleBinary`. Run `make check-wasm` to catch this — it scans the `.wasm` binary for the binding strings.

2. **`Module` must be pre-declared before `leddite_wasm.js` in Emscripten 3.x.** `index.html` has `<script>var Module = {};</script>` before the WASM glue script. Without it, `Module.onRuntimeInitialized` set in `simulator.js` is not reliably picked up, so `ledditeCanvas` is never created and the display stays black. Do not remove that line.

### ESP32 firmware structure

The main sketch (`esp32_firmware/esp32_firmware.ino`) is a mode dispatcher. Each mode is a self-contained class with `begin(canvas)` and `update(canvas)` (and mode-specific `onEncoderTurn`/`onEncoderPress`):

- **MenuMode** — boot menu; encoder navigates, press returns `AppMode` enum
- **TimeMode** — NTP clock (Eastern Time) + scrolling date marquee, DVD-bounces around screen
- **PatternMode** — four patterns (zoom cube tunnel, spinning wireframe pyramid, sparkle, orbiting colour blobs), auto-advance 15s; demo via `demo_3d_patterns.py`
- **TimerMode** — encoder sets minutes 1–90, progress-bar countdown
- **NetworkMode** — WebSocket server; relays binary packets to `Canvas`, broadcasts encoder JSON
- **OctopusMode** — animated character (Pac-Man ghost); encoder cycles 5 colour palettes

Physical LED mapping is column-serpentine: even columns top→bottom, odd columns bottom→top, columns ordered right→left. The mapping is implemented in `getPhysicalIndex(x, y)` in the main `.ino`.

### Python client (`leddite_client.py`)

`LedditeClient` wraps the binary protocol. Key methods: `connect()`, `clear()`, `draw_rect()`, `set_pixel()`, `write_text()`, `send_sprite()`, `listen_encoder()`. The built-in `FONT` dict holds 5×7 bitmap glyphs for A–Z, 0–9, and punctuation. Connect to `localhost:8765` for simulator, `<esp32-ip>:81` for hardware.

### DSL (`dsl_runner.py`)

`LedditeDSLRunner` interprets a simple text DSL: `CLEAR`, `RECT x y w h r g b`, `PIXEL x y r g b`, `TEXT "msg" x y r g b`, `SHOW`, `SLEEP s`, `LOOP n`. Used by `v2_scene_suite.py` for pre-baked demo scenes.
