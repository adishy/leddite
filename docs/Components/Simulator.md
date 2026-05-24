# Simulator

**Role:** High-performance, 16x16 CSS grid UI with WASM-powered logic.

## Key Facts
- **Grid:** 16x16 `div` elements with individual `box-shadow` for "bloom" effects.
- **Logic:** Powered by `leddite_wasm.wasm`, compiled directly from the C++ core (`Canvas`, `Transformer`, `MarqueeEngine`).
- **Communication:** Automatically connects to a local WebSocket server (`ws://localhost:8765`).
- **Refresh Rate:** Optimized for 60 FPS (WASM bridge).
- **Tooling:** Managed by `Makefile` (`make simulator`, `make run-sim`).

## Technical Info
- **WASM Bridge:** `wasm_bridge.cpp` exposes the C++ classes to JavaScript via `emscripten::bind`.
- **Heap Management:** Efficiently shares the 768-byte CRGB buffer between C++ and JS.
