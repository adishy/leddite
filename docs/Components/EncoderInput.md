# EncoderInput

**Role:** Rotary encoder driver — debounced turn detection and button state machine with long-press support.

## Hardware

| Signal | GPIO |
|--------|------|
| CLK    | 32   |
| DT     | 33   |
| Button | 25   |

Uses the **ESP32Encoder** library (madhephaestus).  Attachment order is
`attachFullQuad(DT_PIN, CLK_PIN)` — DT first, CLK second — which matches
the physical wiring.  Reversing these inverts the rotation direction.

## API

```cpp
struct EncoderEvent {
    int  delta;      // +1 CW, -1 CCW, 0 if no turn this poll
    bool pressed;    // true on the poll where a short press is confirmed
    bool released;   // true on the poll after a long-press release
    bool longPress;  // true once, on the poll where the 3 s threshold is crossed
};

class EncoderInput {
public:
    void begin();           // call once in setup()
    EncoderEvent poll();    // call every loop() — returns the event for this tick
};
```

## Turn detection

Raw encoder count is read via `enc.getCount()`.  Dividing by 4 gives
*detents* (the encoder has 4 pulses per physical click).  The delta is
clamped to ±1 per poll to prevent spurious multi-step jumps on fast spins.

## Button state machine

| State | Condition | Event fired |
|-------|-----------|-------------|
| Released | Button goes LOW (pressed) | — |
| Held < 3 s | Button goes HIGH (released) | `pressed = true` |
| Held ≥ 3 s | While still held | `longPress = true` (once) |
| After long-press | Button goes HIGH (released) | `released = true` |

- `DEBOUNCE_MS = 100` ms — ignores transitions shorter than this.
- `LONG_PRESS_MS = 3000` ms.

## Encoder events (Network Canvas mode)

`NetworkMode::handleEncoder()` converts `EncoderEvent` structs into JSON
TEXT WebSocket frames broadcast to all connected clients:

```json
{"type":"encoder","delta":1}           // CW turn
{"type":"encoder","delta":-1}          // CCW turn
{"type":"encoder","button":"pressed"}  // short press
{"type":"encoder","button":"released"} // release after long press
```

Long-press is handled at the `.ino` dispatcher level (returns to menu) and
is not broadcast as a JSON event.

## Simulator equivalent

The browser simulator (`simulator/index.html`) provides equivalent UI controls:
- ↺ CCW / ↻ CW / ⏺ Press buttons
- `←` `→` `Enter`/`Space` keyboard shortcuts
- `L` key simulates a long-press event

These emit the same JSON structure via the WebSocket relay so Python clients
can receive them identically to hardware events.
