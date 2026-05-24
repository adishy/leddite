# MarqueeEngine

**Role:** Internal engine for automated sprite scrolling.

## Key Facts
- **Scrolling Mode:** Left-to-right (from `x=16` to `x=-width`).
- **Speed:** Default 20 pixels per second.
- **Sync:** Synchronized with the local `millis()` (ESP32) or `Date.now()` (Browser).
- **Benefit:** Allows scrolling a large text strip (e.g., 16x64) with only ONE binary packet.
- **WASM Parity:** Compiled into the simulator to ensure sub-pixel-perfect scrolling matches.

## Configuration
- `start(data, w, h, speed, currentTimeMs)`: Begins scrolling.
- `stop()`: Halts scrolling.
- `getXOffset(currentTimeMs)`: Returns the current horizontal offset.
