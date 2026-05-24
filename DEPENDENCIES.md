# Leddite V2 — Dependency Inventory

All third-party dependencies used in source code, build systems, and tooling.
Grouped by context.  Pinned versions are from `requirements.txt`; Arduino library
versions are whatever the IDE installs unless otherwise noted.

---

## ESP32 Firmware (C++ / Arduino)

### Arduino libraries (install via Library Manager or `arduino-cli lib install`)

| Library | Author | Purpose | Install name |
|---------|--------|---------|--------------|
| **FastLED** | Daniel Garcia / FastLED team | WS2812B LED driving, HSV→RGB, `fill_solid`, `CHSV`, `hsv2rgb_rainbow` | `FastLED` |
| **WebSockets** (arduinoWebSockets) | Links2004 (Markus Sattler) | WebSocket server on port 81; binary + text frame support | `WebSockets` |
| **ESP32Encoder** | Kevin Harrington (madhephaestus) | Hardware quadrature encoder decoding on GPIO 32/33 | `ESP32Encoder` |

### Board package

| Package | Maintainer | Purpose |
|---------|-----------|---------|
| **esp32** | Espressif Systems | ESP32 Arduino core — WiFi, NTP (`configTime`/`getLocalTime`), `millis()`, `snprintf`, etc. | Install via Board Manager |

### Standard C / C++ headers used directly

| Header | Source | Usage |
|--------|--------|-------|
| `<Arduino.h>` | ESP32 Arduino core | `millis()`, `delay()`, `HIGH`/`LOW`, `uint8_t`, Serial |
| `<WiFi.h>` | ESP32 Arduino core | WiFi station mode, `WiFi.begin()`, `WiFi.localIP()` |
| `<time.h>` | libc (newlib via ESP-IDF) | `struct tm`, `time()`, `getLocalTime()`, `configTime()`, `setenv()`, `tzset()` |
| `<string.h>` | libc | `memset()`, `memcpy()`, `snprintf()` (via `<stdio.h>`) |
| `<stdio.h>` | libc | `snprintf()` |
| `<stdint.h>` | libc | `uint8_t`, `uint16_t`, `int8_t`, `int16_t`, `uint32_t` |
| `<math.h>` | libc | `sqrt()` (used in PatternMode) |

---

## C++ Core (native / WASM — `src/`, `include/`, `test/`)

No third-party dependencies.  All code compiles with plain `g++` or `emcc`.

Standard headers only:
- `<stdint.h>` — fixed-width integer types
- `<string.h>` — `memset`, `memcpy`
- `<stdio.h>` — `snprintf`
- `<math.h>` — `sqrt`

---

## Build system

### Native unit tests + WASM

| Tool | Version | Purpose |
|------|---------|---------|
| **GNU Make** | system make | `make test`, `make simulator`, `make run-sim`, `make clean` |
| **g++** (Clang on macOS) | system | Compiling native unit tests |
| **Emscripten (emcc)** | latest stable | Compiling C++ core to WebAssembly for the browser simulator |

Emscripten flags used:
- `--bind` — enables `emscripten::bind` for JS↔C++ bindings
- `-s WASM=1` — output `.wasm`
- `-s ALLOW_MEMORY_GROWTH=1` — dynamic heap for variable sprite sizes

### Arduino compile + flash

| Tool | Source | Purpose |
|------|--------|---------|
| **arduino-cli** | Bundled with Arduino IDE 2.x at `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli` | Headless compile (`compile --fqbn esp32:esp32:esp32`) and upload (`upload -p /dev/cu.usbserial-0001`) |

---

## Python (runtime + tooling)

All packages are in `requirements.txt` and installed into `.venv/`.

### Runtime dependencies

| Package | Version | Usage |
|---------|---------|-------|
| **websockets** | 16.0 | Async WebSocket client and server (`websockets.connect`, `websockets.serve`) used in `simulator_server.py`, `test_suite.py`, `leddite_client.py` |
| **python-dotenv** | 1.2.2 | Loading `LLM_API_KEY` from `.env.secrets` in `llm_scenes.py` |
| **google-genai** | 1.73.1 | Gemini API client for LLM scene generation in `llm_scenes.py` |
| **google-ai-generativelanguage** | 0.11.0 | gRPC stubs for Gemini API |
| **google-api-core** | 2.30.3 | Core Google API client infrastructure |
| **google-api-python-client** | 2.194.0 | Google API client base |
| **google-auth** | 2.49.2 | Google authentication |
| **google-auth-httplib2** | 0.3.1 | httplib2 transport for google-auth |
| **grpcio** | 1.80.0 | gRPC transport layer for Gemini |
| **grpcio-status** | 1.75.1 | gRPC status handling |
| **googleapis-common-protos** | 1.74.0 | Common Google API proto definitions |
| **proto-plus** | 1.27.2 | Protocol Buffer Python wrapper |
| **protobuf** | 6.31.1 | Protocol Buffers runtime |
| **httpx** | 0.28.1 | HTTP client (used transitively by google-genai) |
| **httpcore** | 1.0.9 | Low-level HTTP transport for httpx |
| **h11** | 0.16.0 | HTTP/1.1 state machine (used by httpcore) |
| **requests** | 2.33.1 | HTTP (used transitively by google clients) |
| **urllib3** | 2.6.3 | HTTP connection pooling (requests dependency) |
| **certifi** | 2026.4.22 | Mozilla CA bundle |
| **charset-normalizer** | 3.4.7 | Charset detection (requests dependency) |
| **idna** | 3.13 | IDNA 2008 (requests dependency) |
| **httplib2** | 0.31.2 | HTTP client (google-auth-httplib2 dependency) |
| **uritemplate** | 4.2.0 | URI template expansion (google-api-python-client) |
| **pyparsing** | 3.3.2 | Parser combinators (used by httplib2) |
| **tqdm** | 4.67.1 | Progress bars (google-genai / internal use) |
| **tenacity** | 9.1.4 | Retry logic (used transitively) |

### Type / validation dependencies (transitive)

| Package | Version | Usage |
|---------|---------|-------|
| **pydantic** | 2.13.3 | Data validation (google-genai models) |
| **pydantic-core** | 2.46.3 | Rust-backed core for pydantic |
| **annotated-types** | 0.7.0 | Pydantic type annotations |
| **typing-extensions** | 4.15.0 | `typing.Protocol`, `TypeAlias`, etc. |
| **typing-inspection** | 0.4.2 | Runtime type introspection |

### Security / crypto dependencies (transitive)

| Package | Version | Usage |
|---------|---------|-------|
| **cryptography** | 46.0.7 | TLS/crypto primitives (google-auth) |
| **cffi** | 2.0.0 | C FFI for cryptography |
| **pycparser** | 3.0 | C parser (cffi build dependency) |
| **pyasn1** | 0.6.3 | ASN.1 parsing (google-auth) |
| **pyasn1-modules** | 0.4.2 | ASN.1 module definitions |

### Async / concurrency (transitive)

| Package | Version | Usage |
|---------|---------|-------|
| **anyio** | 4.13.0 | Async compatibility layer (httpx) |
| **sniffio** | 1.3.1 | Async library detection (anyio) |

### Misc transitive

| Package | Version | Usage |
|---------|---------|-------|
| **distro** | 1.9.0 | OS detection (google-genai) |

---

## Browser / simulator (JavaScript)

No npm / bundler.  All JS is vanilla, served as static files.

| File | Source | Usage |
|------|--------|-------|
| `leddite_wasm.js` | Generated by Emscripten | WASM loader + glue code; exposes `Module` global |
| `leddite_wasm.wasm` | Generated by Emscripten | Compiled C++ core binary |

No external JS libraries or CDN dependencies.  The simulator works fully offline once the server is running.

---

## Python standard library (no install needed)

Modules used directly:

| Module | Usage |
|--------|-------|
| `asyncio` | Async event loop in `simulator_server.py`, `test_suite.py`, `leddite_client.py` |
| `struct` | Binary packet packing/unpacking in `test_suite.py`, `leddite_client.py` |
| `json` | Encoder event JSON encode/decode |
| `http.server` | SimpleHTTPRequestHandler in `simulator_server.py` |
| `socketserver` | TCPServer in `simulator_server.py` |
| `threading` | HTTP server thread in `simulator_server.py` |
| `math` | `sqrt` in `test_suite.py` (circle rendering) |
| `sys` | CLI args, `sys.exit` |
| `time` | `time.monotonic()` for FPS measurement |
| `os` | `os.path`, `os.environ` |

---

## macOS system tools

| Tool | Purpose |
|------|---------|
| `zsh` | Default shell |
| `git` | Version control |
| `python3` | Venv creation |
| `/dev/cu.usbserial-0001` | USB-serial port to ESP32 |
| `lsof` | Port / process inspection (used in `/e2e-test` skill) |

---

## Direct vs. transitive summary

| Layer | Direct | Transitive |
|-------|--------|-----------|
| ESP32 firmware | FastLED, WebSockets, ESP32Encoder, ESP32 Arduino core | newlib, ESP-IDF |
| C++ core / tests | none | — |
| Build | GNU Make, g++ / Clang, Emscripten, arduino-cli | — |
| Python runtime | websockets, python-dotenv, google-genai | all others in requirements.txt |
| Browser | none | — |
