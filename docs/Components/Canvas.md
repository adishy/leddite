# Canvas (16x16)

**Role:** The persistent internal buffer representing the physical 16x16 LED matrix.

## Key Facts
- **Size:** Exactly 16x16 pixels.
- **Buffer:** Array of 256 `CRGB` (3-byte) structs.
- **Operations:** 
    - `clear()`: Sets all pixels to black.
    - `drawSprite()`: Draws an arbitrary sprite at `(x, y)` with optional `rotation` and `clearBefore`.
- **Bounds Safety:** Automatically clips any pixels drawn outside the 0-15 range.
- **Parity:** Implementation is shared between ESP32 C++ and JS Simulator (via WASM).

## Performance
- **Clear:** ~0.1ms (on ESP32).
- **Full Update:** Optimized for 30+ FPS refresh rates.
