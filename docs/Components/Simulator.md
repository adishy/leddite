# Simulator

**Role:** Browser-based 16×16 LED display simulator with 100% C++ logic parity and rotary encoder emulation.

## Key Facts
- **Grid:** 16×16 `div` elements, each styled as a circular LED with `box-shadow` bloom.
- **Logic:** Powered by `leddite_wasm.wasm` compiled from the C++ core (`Canvas`, `Transformer`, `MarqueeEngine`) via Emscripten.
- **Transport:** Auto-connects to the relay server at `ws://localhost:8765`.
- **Encoder UI:** Three buttons (↺ CCW, ⏺ Press, ↻ CW) emit JSON encoder events over the WebSocket, identical to what the ESP32 hardware sends in Network Canvas mode.
- **Keyboard shortcuts:** `←`/`→` = CCW/CW turn, `Enter`/`Space` = press, `L` = long-press simulation, `C` = clear.
- **Refresh rate:** 60 FPS via `requestAnimationFrame`.

## Architecture

```
Browser (simulator/index.html + simulator.js)
    │  ← binary sprite frames (ArrayBuffer)
    │  ← JSON encoder events relayed from other clients
    │  → JSON encoder events (button clicks / keyboard)
    ▼
simulator_server.py  ws://localhost:8765  (relay — broadcasts all messages to all peers)
    │
    ├── test_suite.py / leddite_client.py  (sends binary frames, receives encoder JSON)
    └── hardware ESP32 on port 81          (separate connection — use test_suite.py <ip> 81)
```

## Running

```bash
# Terminal 1 — start relay + HTTP server
python simulator_server.py

# Open browser
open http://localhost:8000

# Terminal 2 — run test suite against simulator
python test_suite.py

# Visual verify mode (pauses between tests for inspection)
python test_suite.py --verify
```

## Encoder simulation

The simulator's encoder UI sends JSON TEXT frames to the relay server, which
broadcasts them to all other connected clients.  This lets `test_suite.py` or
`leddite_client.py` receive encoder events from the browser exactly as they
would from real hardware.

| Action | JSON sent |
|--------|-----------|
| ↻ CW button / `→` key | `{"type":"encoder","delta":1}` |
| ↺ CCW button / `←` key | `{"type":"encoder","delta":-1}` |
| ⏺ Press button / `Enter`/`Space` | `{"type":"encoder","button":"pressed"}` |
| Release after press | `{"type":"encoder","button":"released"}` |
| `L` key | `{"type":"encoder","longPress":true}` |

## Rebuilding the WASM module

The pre-built `.wasm` and `.js` files are committed. Rebuild after changing
any file under `src/` or `include/`:

```bash
# Requires Emscripten (emcc) on PATH — see /work-setup
make simulator
```

## Technical Details
- **WASM bridge:** `simulator/wasm_bridge.cpp` — Emscripten `EMSCRIPTEN_BINDINGS` exposing `Canvas` as a JS class with `drawSprite`, `startMarquee`, `updateMarquee`, `stopMarquee`, `isMarqueeActive`, `getBuffer`, `clear`.
- **Heap sharing:** pixel data is allocated on the WASM heap (`_malloc`/`_free`) for zero-copy transfer between JS and C++.
- **Relay server:** `simulator_server.py` is a pure relay — it does not interpret packets. Any connected client can send to any other.
- **Tooling:** `Makefile` targets — `make simulator` (WASM build), `make run-sim` (WASM + HTTP + WS server).
