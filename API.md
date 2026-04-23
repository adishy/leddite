# Leddite V2 Binary API Specification

## Protocol Overview
Leddite V2 uses a high-performance, binary "Sprite-based" protocol designed for low-latency updates of a 16x16 RGB matrix.

## The Header (8 Bytes)
Every packet MUST start with an 8-byte header:

| Offset | Field | Type | Description |
|---|---|---|---|
| 0 | `version` | `uint8_t` | Protocol version (currently `1`). |
| 1 | `flags` | `uint8_t` | Bitmask for control (see below). |
| 2 | `width` | `uint8_t` | Sprite width in pixels. |
| 3 | `height` | `uint8_t` | Sprite height in pixels. |
| 4 | `x_offset` | `int8_t` | Starting X coordinate on the canvas. |
| 5 | `y_offset` | `int8_t` | Starting Y coordinate on the canvas. |
| 6 | `rotation` | `uint8_t` | Rotation code: 0=0°, 1=90°, 2=180°, 3=270°. |
| 7 | `brightness`| `uint8_t` | Master brightness (0-255). |

### Flags (Byte 1)
- `Bit 0 (0x01)`: **Clear Canvas** - If set, the 16x16 canvas is cleared to black before drawing the sprite.
- `Bit 1 (0x02)`: **Show Immediately** - If set, `FastLED.show()` is called after drawing.
- `Bit 2 (0x04)`: **Marquee Active** - If set, the sprite will automatically scroll if its dimensions exceed the canvas.

## The Payload
Immediately following the header is the RGB pixel data.
- **Length:** `width * height * 3` bytes.
- **Order:** `[R, G, B, R, G, B, ...]` (row-major).

## Transformation Logic
- **Rotation:** Per-sprite rotation is applied *before* the offset is calculated.
- **Clipping:** Pixels outside the 16x16 bounds are automatically clipped and do not corrupt memory.
- **Layering:** By clearing `Bit 0` in the flags, you can layer multiple sprites (e.g., a background sprite followed by a moving icon sprite).

## Performance Notes
- A full-frame 16x16 update requires `8 + (16*16*3) = 776 bytes`.
- At 30 FPS, this is ~186 Kbps, well within ESP32 WebSocket/UDP limits.
- Small sprites (e.g., 5x5 icons) require only `8 + (5*5*3) = 83 bytes`.
