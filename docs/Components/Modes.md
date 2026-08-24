# Modes

**Role:** The five application modes selectable from the boot menu, plus the OFF pseudo-mode.

All modes are C++ classes in `esp32_firmware/`.  The main loop in
`esp32_firmware.ino` dispatches encoder events and calls `update()` on the
active mode every iteration, then calls `updateDisplay()` to push the shared
`Canvas` buffer to FastLED.

## AppMode enum (`AppState.h`)

```cpp
enum class AppMode {
    MENU,       // Boot menu — navigate with encoder, press to select
    CLOCK_CAL,  // Clock + Calendar
    NETWORK,    // Network Canvas (WebSocket binary API)
    PATTERN,    // Pattern Slideshow
    TIMER,      // Visual Timer
    OCTOPUS,
    GAMES,
    SETTINGS,    // Characters — animated ghost, press cycles colour style
    OFF,        // Screen blank — short press wakes
};
```

---

## Boot Menu (`MenuMode`)

Displayed immediately after WiFi + NTP init.

- **Display:** Full mode name scrolls left via `MarqueeEngine` (18 px/s).
  Five indicator dots at y=14, x=2/5/8/11/14 in per-mode accent colors.
- **Colors:**
  | Mode | Label | Color | RGB |
  |------|-------|-------|-----|
  | CK | Clock+Cal | Golden amber | {255,200,80} |
  | NT | Network | Warm orange | {255,130,40} |
  | PT | Pattern | Coral rose | {255,90,90} |
  | TM | Timer | Warm yellow | {255,230,100} |
  | OC | Characters | Ocean teal | {40,220,210} |
- **Encoder:** Turn → cycle modes; Press → enter mode.
- **Long press (3 s):** Screen off (`AppMode::OFF`).
- **Wake from OFF:** Short press → back to menu.

---

## Clock + Calendar (`TimeMode`)

NTP-synchronized 24-hour clock alternating with a date display, both animated
as a DVD screensaver — the block drifts one pixel per second and bounces off
the canvas walls.

### Clock view
- Top row: `HH` (24-hour) in sky-blue `{80,180,255}`.
- Bottom row: `MM` in pink `{255,80,160}`.
- Block is 12×15 px; bounces within x=0..4, y=0..1.
- Redraws once per second (position steps with each new second).
- Falls back to `--` / `--` if NTP is not yet synchronized.

### Calendar view
- Top row: `DD` (day-of-month) in warm orange `{255,160,50}`.
- Bottom row: 3-char month abbreviation `JAN`..`DEC` in soft green `{80,220,120}`.
- Month text is 18 px wide — slightly wider than the 16 px canvas; the block
  bounces x=-2..+2 so both edges clip briefly in turn (no character is
  permanently hidden).

### Switching
- Auto-switches every 10 s.
- Short press: skip to the other view immediately.
- Long press (3 s): back to menu.

---

## Network Canvas (`NetworkMode`)

Exposes the WebSocket binary sprite API on port 81.  The existing protocol is
**100% unchanged** — all existing clients (`leddite_client.py`, `test_suite.py`,
`llm_scenes.py`, `dsl_runner.py`) work without modification.

### Binary protocol (recap)
8-byte header + raw RGB payload.  See `docs/Components/Protocol.md`.

### Encoder events
While in this mode, encoder input is broadcast as JSON TEXT frames to all
connected WebSocket clients:
```json
{"type":"encoder","delta":1}           // CW turn
{"type":"encoder","delta":-1}          // CCW turn
{"type":"encoder","button":"pressed"}  // short press
{"type":"encoder","button":"released"} // release after long press
```

Receive with `leddite_client.py`:
```python
await client.listen_encoder(my_callback)
```

### Returning to menu
Long press (3 s) → back to boot menu.

---

## Pattern Slideshow (`PatternMode`)

Four self-contained patterns cycling automatically.

| # | Name | Description |
|---|------|-------------|
| 0 | Rainbow Wave | Per-pixel HSV hue from x/y/time, 30 FPS |
| 1 | Lava Lamp | Two oscillating color blobs, distance-based brightness |
| 2 | Pulse | Full-canvas hue 160 (teal) pulsing via `sin8` |
| 3 | Sparkle | Random white sparks decaying over dark blue background |

- **Auto-advance:** 15 s per pattern.
- **Encoder turn or short press:** Skip to next pattern immediately.
- **Long press (3 s):** Back to menu.

---

## Visual Timer (`TimerMode`)

State machine: `SET_MINS → RUNNING → FINISHED`.

### SET_MINS
- Displays current minute count (01–90) in cornflower blue `{100,149,237}`, centered.
- Encoder turn: adjust minutes (±1, clamped 1–90).
- Short press: start countdown → `RUNNING`.

### RUNNING
- Progress bar: fills logical pixels 0..N proportional to elapsed/total,
  rainbow-hued (hue increments each frame).
- Short press: cancel → back to menu.

### FINISHED
- Full canvas rainbow blink (on/off every 300 ms).
- Short press: back to menu.

### Long press (3 s) — any state
Back to menu immediately.

---

## Characters (`OctopusMode`)

Animated character display.  Currently one character (Ghost); the architecture
supports adding more by incrementing `NUM_CHARS` and adding a draw function.

### Ghost
A Pac-Man-style ghost centered on the 16×16 canvas.

- **Body:** 9-row dome shape with a vertical gradient top→bottom.
- **Eyes:** White sclera (3×3 px each), black pupils (2×2 px).
- **Skirt legs:** 4 angled legs fanning out from the bottom.
- **Animation:** Idle → bob up/down (8 frames at 110 ms/frame) → idle pause
  (1.8–3.4 s random) → repeat.  Eyes blink briefly during idle.

**Colour palettes (short press cycles):**

| # | Name | Body colour |
|---|------|-------------|
| 0 | Blinky | Red |
| 1 | Pinky | Pink |
| 2 | Inky | Cyan |
| 3 | Clyde | Orange |
| 4 | Scared | Blue |

### Encoder
- **Turn:** Cycle character (no-op with one character; extend `NUM_CHARS` to add more).
- **Short press:** Cycle colour palette within the current character.
- **Long press (3 s):** Back to menu.

---

## OFF

Not a mode in the traditional sense — it is entered via long-press from the
boot menu.

- `FastLED.clear(true)` is called immediately; the main loop returns before
  `updateDisplay()`, so the canvas is never re-pushed.
- Short press: `goToMenu()` → boot menu.
- Long press while OFF: ignored.


---

## Game Screensavers (`AppMode::GAMES`)

Entered from the boot menu. Driven by `UiMode` → `src/UiController`, so the same
code runs in the browser simulator (see `docs/adr/0002`).

### Submenu

Three visible rows of `SmallTextRenderer`'s 3×4 font (`ListMenu`):

```
 y  0- 3   row 0        4px glyph band
 y     4   gutter
 y  5- 8   row 1
 y     9   gutter
 y 10-13   row 2
 y    14   gutter
 y    15   position bar — proportional thumb, tracks the selection
```

The selected row carries a band in the item's accent colour with bright text on
top; unselected rows are dim and clipped. Only the selected row scrolls, and only
if its label exceeds 16px — after a 1.2 s dwell so the opening characters are
readable. At a 4px advance per character, labels of 4 characters or fewer never
scroll.

| Entry | Width | Scrolls |
|-------|-------|---------|
| `SNAKE` | 20px | yes |
| `LIFE` | 16px | no |
| `INVADERS` | 32px | yes |
| `DINO` | 16px | no |
| `CYCLE ALL` | 33px | yes |

### Games

All are auto-playing and endless — there are no game-over screens, and losing
states soft-reset into a fresh round.

| Game | Behaviour |
|------|-----------|
| **Snake** | Greedy toward food with a 25% random legal move so the path does not look robotic. Never enters its own body; walls are excluded from the legal move set rather than fatal, so it bumps and turns. Soft-respawns if boxed in. |
| **Life** | Conway on a 32×32 torus viewed through the 16×16 panel. A decayed heatmap of cell changes steers a camera toward wherever the most is happening. Reseeds when population drops below 20. |
| **Invaders** | 4×3 formation marching at ⅓ the bullet rate, dropping a row at each wall. The cannon tracks the lowest surviving invader and fires on alignment. Clearing the field or reaching the cannon starts a fresh wave. |
| **Dino** | Endless runner. Jump is triggered by speed-relative lookahead, not reaction, so every obstacle is cleared. Day/night palette flip; speed ramps then resets. |
| **Cycle All** | Advances through all four every 20 s. |

### Encoder

- **Turn** — move the selection; while playing, skip to the next game
- **Short press** — launch the selected game; while playing, restart the round
- **Long press (3 s)** — *up one level*: a running game returns to the game list;
  the game list returns to the boot menu

---

## Settings (`AppMode::SETTINGS`)

Same `ListMenu` presentation. Values apply live and are persisted to NVS
(namespace `leddite`) when confirmed or backed out of.

| Item | Editor |
|------|--------|
| `BRIGHTNESS` | Level 1–10 shown as a number over a proportional bar. The panel's own brightness changes as you turn, so the screen is its own preview. |
| `PLACE` | The `Places` table — `NYC`, `CAMB`, `SFO`, `SLL`, `MCT`, `AMH`, `TVM`. Changing it invalidates the cached weather and refetches immediately. |
| `UNITS` | Celsius or Fahrenheit. Readings are always fetched in Celsius and converted at render time, so the toggle is instant and works offline. |

### Brightness and power

Levels map through an explicit table to FastLED brightness 20–200. Level 10 is
deliberately not 255: 256 WS2812B pixels at full white draw roughly 15 A.

| Level | FastLED | Worst case (all white) |
|-------|---------|------------------------|
| 1 | 20 | 1.20 A |
| 4 | 66 | 3.98 A |
| 7 | 132 | 7.95 A |
| 10 | 200 | 12.05 A → limited |

The table bounds the scale factor, not frame content, so the firmware also calls
`FastLED.setMaxPowerInVoltsAndMilliamps(5, 8000)`. Both are required.

---

## Weather (part of `TimeMode`)

The first boot-menu entry cycles clock → date → weather every 10 s.

`WeatherClient` fetches from **Open-Meteo**, which needs no API key and takes
lat/lon directly, so nothing is stored on the device. It runs on its own FreeRTOS
task because a blocking HTTPS call on the main loop would freeze the display.

- **Refresh:** every 45 min (~32 requests/day), exponential backoff to a 15 min
  ceiling on failure. Refreshing on view entry would be ~8,600 requests/day since
  the views rotate every 10 s — the cached reading is served instead.
- **Rendering:** `src/WeatherView`, which takes a plain struct and does no
  networking, so icon selection, unit conversion and layout are all unit-tested.

### Icons

Eleven procedurally drawn shapes cover ~25 WMO codes. Intensity becomes the
number of precipitation streaks rather than separate icons, and freezing variants
are a palette swap.

| Icon | WMO codes |
|------|-----------|
| `CLEAR_DAY` / `CLEAR_NIGHT` | 0, 1 (by `is_day`) |
| `PARTLY_DAY` / `PARTLY_NIGHT` | 2 |
| `CLOUDY` | 3 |
| `FOG` | 45, 48 |
| `DRIZZLE` | 51–57 |
| `RAIN` | 61–67, 80–82 |
| `SNOW` | 71–77, 85, 86 |
| `THUNDER` | 95, 96, 99 |
| `UNKNOWN` | anything else, **and the fetch-failure state** |

`UNKNOWN` doubling as the failure state is deliberate: a failed fetch must never
render as though it were a live reading.

### Layout

Icon on rows 0–9, temperature on rows 11–14. The place code flashes for 2 s on
entry. There is **no degree symbol** — at a 4px advance `-12*C` is 18px and would
scroll on a 16px row. Without it every realistic reading fits, with `-40C` and
`-60C` landing at exactly 16px.
