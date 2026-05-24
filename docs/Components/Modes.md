# Modes

**Role:** The four application modes selectable from the boot menu, plus the OFF pseudo-mode.

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
    OFF,        // Screen blank — short press wakes
};
```

---

## Boot Menu (`MenuMode`)

Displayed immediately after WiFi + NTP init.

- **Display:** Full mode name scrolls left via `MarqueeEngine` (18 px/s).
  Four indicator dots at y=14, x=3/6/9/12 in per-mode warm accent colors.
- **Colors:**
  | Mode | Color | RGB |
  |------|-------|-----|
  | CK | Golden amber | {255,200,80} |
  | NT | Warm orange | {255,130,40} |
  | PT | Coral rose | {255,90,90} |
  | TM | Warm yellow | {255,230,100} |
- **Encoder:** Turn → cycle modes; Press → enter mode.
- **Long press (3 s):** Screen off (`AppMode::OFF`).
- **Wake from OFF:** Short press → back to menu.

---

## Clock + Calendar (`TimeMode`)

NTP-synchronized 24-hour clock alternating with a scrolling date display.

### Clock view
- Hours row (y=1): `HH` in sky blue `{80,180,255}`, centered.
- Minutes row (y=9): `MM` in pink `{255,80,160}`, centered.
- Redraws only when the minute changes (no flicker).
- Falls back to `--` / `--` if NTP is not yet synchronized.

### Calendar view
- Builds `"DD MMM YYYY"` string (e.g. `"24 MAY 2026"`).
- Scrolls as a mint-green `{80,255,120}` marquee at 30 px/s.

### Switching
- Auto-switches every 5 s.
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

## OFF

Not a mode in the traditional sense — it is entered via long-press from the
boot menu.

- `FastLED.clear(true)` is called immediately; the main loop returns before
  `updateDisplay()`, so the canvas is never re-pushed.
- Short press: `goToMenu()` → boot menu.
- Long press while OFF: ignored.
