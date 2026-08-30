# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **Read `RULES.md` first.** It carries the binding repo rules — chiefly that
> compiled build artifacts are never committed (`make check-artifacts` enforces
> it; `simulator/leddite_wasm.*` is the one documented exception), and that new
> mode logic belongs in `src/`, not `esp32_firmware/` (see `docs/adr/0009`).
>
> `docs/adr/` records the significant architecture decisions and their costs —
> start at [`docs/adr/README.md`](docs/adr/README.md).

## What this project is

Leddite V2 is a 16×16 WS2812B LED matrix driven by an ESP32, with a binary WebSocket API for programmatic control, a WASM-powered browser simulator, and a Python client library. The codebase has three distinct layers: ESP32 firmware (C++/Arduino), a native-compilable C++ core (`src/` + `include/`), and Python tooling.

## Commands

### Python environment

```bash
uv venv && uv pip install -r requirements.txt   # preferred
# or: python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

### C++ unit tests (no Arduino/hardware needed)

```bash
make test                        # build + run all 10 test binaries
make test-wasm                   # drive the committed WASM DeviceUI under node
make check-artifacts             # fail if a compiled binary is tracked (RULES.md)
make test-text-renderer          # single test binary
./test/test_canvas               # run one already-built binary
```

Test binaries are gitignored build artifacts — see `RULES.md` §1. They were once
committed as macOS builds and broke `make test` on Linux.

### Simulator (browser-based, WASM)

```bash
.venv/bin/python simulator_server.py   # HTTP :8000 + WS relay :8765
# open http://localhost:8000 in browser
make simulator                         # rebuild WASM (requires Emscripten) + verifies bindings
make check-wasm                        # verify committed WASM has Canvas bindings (no rebuild)
make run-sim                           # rebuild WASM then start server
```

**Must rebuild WASM after any change to `src/`, `include/`, or `simulator/wasm_bridge.cpp`**, then commit the new `simulator/leddite_wasm.js` + `simulator/leddite_wasm.wasm`. Run `make check-wasm` to verify a committed WASM is valid without rebuilding.

The simulator has two render modes. **Network** draws frames arriving over the
WebSocket protocol. **Device UI** runs the real firmware state machine
(`src/UiController`) compiled to WASM, so the games, submenus, brightness editor
and weather view in the browser are the same code the ESP32 executes — not a
JS reimplementation. See `docs/adr/0010`.

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

### ESP32 firmware

Requires `arduino-cli`, the `esp32:esp32` core, and four libraries:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install FastLED WebSockets ESP32Encoder ArduinoJson
```

Keep the `esp32_firmware/` copies of the `src/` modules in step first, then build:

```bash
tools/sync-firmware-copies.sh                    # copy src/ + include/ -> esp32_firmware/
tools/sync-firmware-copies.sh --check            # fail if any copy is stale

arduino-cli compile -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino
arduino-cli upload -p /dev/ttyUSB0 -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino
```

On macOS the port is `/dev/cu.usbserial-0001`, and `arduino-cli` ships inside
Arduino IDE 2.x at
`/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`.

**Build with `PartitionScheme=min_spiffs`** — it is not optional any more:

```bash
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs esp32_firmware/esp32_firmware.ino
arduino-cli upload  -p /dev/ttyUSB0 -b esp32:esp32:esp32:PartitionScheme=min_spiffs esp32_firmware/esp32_firmware.ino
```

That takes each OTA app slot from 0x140000 to 0x1E0000, and this build from 91%
of a slot to **61%**. Omitting the flag silently reverts to the smaller layout.
Nothing here uses SPIFFS, and `nvs` is at 0x9000/0x5000 in both schemes so saved
settings survive the switch. **The first flash after this change must be over
USB** — a partition table is not part of an OTA payload. See `docs/adr/0014`
(partitions and rollback) and `docs/adr/0015` (the update flow itself).

WiFi credentials go in `esp32_firmware/wifi_credentials.h` (gitignored). Generate
it from `LEDDITE_SSID` / `LEDDITE_PASSWORD` without echoing the values:

```bash
tools/gen-wifi-credentials.sh [path-to-env-file]
```

### OTA updates

`Settings → UPDATE` opens a **browser upload window**, it does not fetch
(`docs/adr/0015`; `docs/adr/0014` is the superseded pull). The confirmation always
opens on **NO**. Turning to YES starts an HTTP server on port 80 and scrolls the
device's URL — `HTTP://192.168.0.113` — across the panel. You browse to it, drop a
`.bin` on the page, and it is written to the other app slot.

```bash
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --output-dir build/fw esp32_firmware/esp32_firmware.ino
# then: Settings → UPDATE → YES at the panel, browse to the URL it shows,
# and upload build/fw/esp32_firmware.ino.bin
```

There is nothing to configure and no image server to run. `tools/gen-ota-config.sh`
and `ota_config.h` are gone; the version string is `LEDDITE_FW_VERSION` in
`OtaUpdater.cpp`, overridable with a build property.

**The window is deliberately short-lived.** It closes on the next press, on a
long-press, on completion, and after `UiController::OTA_WINDOW_MS` (5 min). An
upload server that stayed up for the device's whole uptime would accept firmware
from anyone on the LAN — the window is what keeps physical presence as the
authentication factor.

**No firewall rule is needed on your machine.** The connection runs browser →
device. The earlier pull needed the reverse and never worked here: the panel
reaches its gateway in 8 ms and `1.1.1.1` in 20 ms but times out against the build
host every time, because it sits one router hop away (host→gateway `ttl=64`,
host→device `ttl=63`) and its ARP for an apparently on-link host never crosses
that hop. A `ufw allow` was added on a wrong diagnosis and changed nothing.

`Settings → IP` shows the same address on its own, for the times you want it
without opening an update window.

A freshly written image is on probation: `OtaUpdater::tick()` marks it valid only
after 30 s of connected uptime, so an image that fails to boot rolls back to the
previous slot by itself. The boot banner prints the version and the running slot,
which is the only way to tell a successful update from a silent no-op.

## Architecture

### The dual C++ problem

The core rendering logic lives in **two places** that must stay in sync:

| Layer | Location | Compiled with |
|-------|----------|---------------|
| ESP32 production | `esp32_firmware/*.cpp` (copies) | arduino-cli (Arduino framework) |
| Native / WASM | `src/` (same filenames) + `include/` headers | `g++` (unit tests) / `emcc` (WASM) |

Duplicated modules: `Canvas`, `Transformer`, `MarqueeEngine`, `ProtocolHandler`,
`TextRenderer`, plus the mode-logic set — `ColorUtils`, `Draw`,
`SmallTextRenderer`, `ListMenu`, `GameEngine`, `BrightnessModel`, `WeatherView`,
`Places`, `UiController`.

When editing any of these, update **both** copies — run
`tools/sync-firmware-copies.sh` (and `--check` in CI) rather than copying by
hand. The `src/` copies have no Arduino dependencies, making them unit-testable
and WASM-compilable.

**New mode logic must go in `src/`, not `esp32_firmware/`** — Arduino-free, with
time and randomness injected. See `docs/adr/0009` for why, and `RULES.md` §3.

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
- **TimeMode** — NTP clock (Eastern Time), date, and current weather; cycles every 10 s, DVD-bounces around screen (the weather face does not bounce — it shows the temperature over a scrolling condition description)
- **TimerMode** — "TIMER" in the boot menu; encoder sets minutes 1–90, progress-bar countdown
- **NetworkMode** — WebSocket server; relays binary packets to `Canvas`, broadcasts encoder JSON
- **OctopusMode** — animated character (Pac-Man ghost); encoder cycles 5 colour palettes
- **UiMode** — thin Arduino shell around `src/UiController`, which owns the Games
  and Settings submenu trees. Supplies `millis()`, pushes the rendered buffer to
  `Canvas`, and mirrors brightness/place/unit into NVS via `Preferences`.
  The games are Snake, Invaders, Dino, Pong and Breakout (`Game` enum order,
  which `GAME_ITEMS` in `UiController.cpp` must match). Their headline
  behaviours — the dino never hitting a cactus, Invaders' 0.3 win rate — are
  explicit invariants with tests, not tuning; see `docs/adr/0013`.
- **WeatherClient** — Open-Meteo (no API key) on its own FreeRTOS task, so a
  blocking HTTPS call never stalls the display. Fetches every 45 min with
  exponential backoff; always in Celsius, with conversion done at render time so
  the units toggle is instant and works on cached data.

### Brightness and power

`BrightnessModel` maps user levels 1–10 onto FastLED brightness 6–200 through an
explicit table. That bounds the *scale factor* but not frame *content*: at levels
8–10 an all-white frame would draw 9.4–12.0 A against an 8 A budget. The firmware
therefore also calls `FastLED.setMaxPowerInVoltsAndMilliamps(5, 8000)`, which
scales each frame by what it actually contains. **Both are required.** Packet
brightness in `NetworkMode` is additionally capped at the user's level so a remote
client cannot exceed it.

Physical LED mapping is column-serpentine: even columns top→bottom, odd columns bottom→top, columns ordered right→left. The mapping is implemented in `getPhysicalIndex(x, y)` in the main `.ino`.

### Python client (`leddite_client.py`)

`LedditeClient` wraps the binary protocol. Key methods: `connect()`, `clear()`, `draw_rect()`, `set_pixel()`, `write_text()`, `send_sprite()`, `listen_encoder()`. The built-in `FONT` dict holds 5×7 bitmap glyphs for A–Z, 0–9, and punctuation. Connect to `localhost:8765` for simulator, `<esp32-ip>:81` for hardware.

### DSL (`dsl_runner.py`)

`LedditeDSLRunner` interprets a simple text DSL: `CLEAR`, `RECT x y w h r g b`, `PIXEL x y r g b`, `TEXT "msg" x y r g b`, `SHOW`, `SLEEP s`, `LOOP n`. Used by `v2_scene_suite.py` for pre-baked demo scenes.
