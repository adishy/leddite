# Hardware Mapping

**Role:** Translates logical canvas coordinates `(x, y)` to the physical WS2812B LED index.

## Key Facts

- **Panel:** 16×16 WS2812B matrix (256 LEDs total), connected to **GPIO 4**.
- **Color order:** `GRB` (standard for WS2812B; FastLED handles the swap automatically).
- **Brightness:** Default 64 / 255 (~25%). Overridable per-packet via `header.brightness`.
- **Mounting:** The panel is physically mounted **90° rotated** inside the enclosure. The serpentine data chain runs **column-by-column**, not row-by-row.

## Physical Serpentine Layout

The LED chain starts at the **bottom-right corner** of the display (as viewed from the front) and snakes column-by-column from right to left:

```
Visual display (front view):
x=0          x=15
↓              ↓
col 0  col 1  …  col 15   ← visual columns
  ↓      ↑           ↓
y=0   y=0          y=15   ← LED 0 is at (15,15) = bottom-right
…     …            …
y=15  y=15         y=0
```

| Physical column | Visual x | Direction | LED indices |
|-----------------|----------|-----------|-------------|
| 0 (rightmost)   | x = 15   | bottom→top | 0 – 15      |
| 1               | x = 14   | top→bottom | 16 – 31     |
| 2               | x = 13   | bottom→top | 32 – 47     |
| …               | …        | alternates | …           |
| 15 (leftmost)   | x = 0    | top→bottom | 240 – 255   |

**Rule:** even-numbered visual columns (x=0,2,4,…) have their chain segment running top→bottom; odd columns (x=1,3,5,…) run bottom→top.

## `getPhysicalIndex` Formula

```cpp
uint16_t getPhysicalIndex(uint8_t x, uint8_t y) {
    if (x % 2 == 0) return (15 - x) * 16 + y;         // top → bottom
    else             return (15 - x) * 16 + (15 - y);  // bottom → top
}
```

This produces a **bijection** — each of the 256 canvas positions maps to exactly one unique physical LED, and all 256 LEDs are covered.

## Discovery Process

The mapping was confirmed via a dedicated diagnostic firmware (`fix(firmware): mapping diagnostic` commit) that:

1. Used `fill_solid()` directly (zero custom mapping) to confirm all 256 LEDs respond.
2. Lit single pixels at each corner (`(0,0)`, `(15,0)`, `(0,15)`, `(15,15)`) with distinct colors.
3. Scanned individual rows and columns to verify direction.
4. Lit raw `leds[0]` and `leds[0..15]` to identify the chain start and first-band direction.

Cross-reference: the same transform was used in the legacy `esp32_visual_timer` firmware as `XY_Serpentine_Original()` + 90°+180° flip inside `setPixel_Final()`.

## Validation

To re-validate after any physical change (remounting, new panel, new cable):

1. Flash the mapping diagnostic firmware: look for the commits tagged `mapping diagnostic` in `v2-refactor`.
2. Confirm corners: `(0,0)` = top-left green, `(15,0)` = top-right red, `(0,15)` = bottom-left blue, `(15,15)` = bottom-right yellow.
3. Confirm row scan moves top→bottom and column scan moves left→right.
4. Confirm `leds[0]` (raw) lights the bottom-right corner.
