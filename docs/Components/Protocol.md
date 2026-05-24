# Protocol (Binary)

**Role:** High-performance, binary "Sprite-based" communication.

## Key Facts
- **Header:** Exactly 8 bytes.
- **Payload:** Raw `(width * height * 3)` bytes of RGB data.
- **Max Throughput:** 30+ FPS (full-frame 16x16) at ~186 Kbps.

## Header Structure
- `[0]` **Version:** `1`
- `[1]` **Flags:** 
    - `0x01`: Clear Canvas before draw.
    - `0x02`: Show immediately (`FastLED.show()`).
    - `0x04`: Marquee active (internal scrolling).
- `[2]` **Width:** Sprite width.
- `[3]` **Height:** Sprite height.
- `[4]` **X-Offset:** `int8_t` (supports negative).
- `[5]` **Y-Offset:** `int8_t` (supports negative).
- `[6]` **Rotation:** `0-3` (0°, 90°, 180°, 270°).
- `[7]` **Brightness:** `0-255`.

## Usage Note
A full-frame update (16x16) requires `8 + 768 = 776 bytes`.
A 5x5 icon update requires only `8 + 75 = 83 bytes`.
