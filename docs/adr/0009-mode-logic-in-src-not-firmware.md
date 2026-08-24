# ADR 0009 — Mode logic belongs in `src/`, not `esp32_firmware/`

- **Status:** Accepted
- **Date:** 2026-08-23
- **Context branch:** `v2-game-screensavers-settings`
- **Extends:** [ADR 0003](0003-native-compilable-core.md) to the mode layer
- **Corrects:** [ADR 0008](0008-boot-menu-mode-dispatcher.md), which put modes outside the testable core

## Context

[ADR 0003](0003-native-compilable-core.md) established a native-compilable core so
rendering could be unit-tested. [ADR 0008](0008-boot-menu-mode-dispatcher.md) then
introduced modes — and wrote every one of them directly against `Arduino.h` and
`FastLED`, outside that core. This ADR closes that gap for new work.

Leddite's C++ is split across two trees that are kept in sync by hand:

| Tree | Compiled by | Testable |
|------|-------------|----------|
| `src/` + `include/` | `g++` (unit tests), `emcc` (WASM simulator) | yes |
| `esp32_firmware/` | `arduino-cli` | no |

Only five files live in both: `Canvas`, `Transformer`, `MarqueeEngine`,
`ProtocolHandler`, `TextRenderer`.

Every **mode** class — `MenuMode`, `TimeMode`, `PatternMode`, `TimerMode`,
`NetworkMode`, `OctopusMode` — exists *only* under `esp32_firmware/` and pulls in
`Arduino.h` and `FastLED.h`. Consequences observed before this change:

- No mode has a unit test. There is no way to write one: the code cannot be
  compiled natively.
- No mode can be seen in the simulator. The simulator only renders binary
  protocol frames arriving over WebSocket, and modes never emit those.
- To preview `PatternMode`, someone hand-wrote `demo_3d_patterns.py` — a third,
  independent implementation of the same patterns in Python. Nothing keeps it in
  sync with the C++, and any divergence is invisible.
- Bugs are therefore found by flashing hardware and looking at it.

Adding four auto-playing games, two submenus, a settings editor and a weather
view along the existing grain would have multiplied all four problems.

## Decision

**New mode logic is written Arduino-free in `src/` + `include/`, with time and
randomness injected.** The `esp32_firmware/` side keeps only a thin wrapper that
supplies `millis()` and pushes the resulting buffer through
`canvas.drawSprite()`.

Concretely, every new core module:

- includes no `Arduino.h`, no `FastLED.h`, and calls neither `millis()` nor
  `random()`;
- takes `nowMs` as a parameter on `update()` / `render()`;
- seeds its own deterministic `xorshift32` rather than using `random()`;
- renders into a flat `16 * 16 * 3` byte buffer, the same layout
  `Canvas::drawSprite()` and the WASM bridge already consume.

This required porting the FastLED maths the modes depend on — `sin8`, `cos8`,
`scale8` and `hsv2rgb_rainbow` — into `ColorUtils`, so native, WASM and ESP32
builds produce **byte-identical** pixels.

### Modules introduced

| Module | Responsibility |
|--------|----------------|
| `ColorUtils` | FastLED maths ports (`sin8`/`cos8`/`scale8`/`hsv2rgb_rainbow`) |
| `Draw` | Clipped primitives over the 768-byte frame buffer |
| `SmallTextRenderer` | 3x4 proportional font — three text rows fit on a 16x16 |
| `ListMenu` | Reusable vertical submenu: accent band, viewport panning, position bar |
| `GameEngine` | Snake, Life, Invaders, Dino — deterministic, endless |
| `Places` | Fixed location table shared by tests, simulator and firmware |

## Consequences

### Positive

- Games and menus are unit-testable. The first soak run over the `GameEngine`
  immediately found two real bugs that would otherwise have reached hardware:
  the Invaders formation was 13px wide in a 16px field, so it bounced every
  ~3 steps and rained to the bottom before the cannon could land a single shot;
  and the Dino's `obsUsed` counter leaked, saturating at `OBS_MAX` and silently
  stopping all obstacle spawning after ~34 steps.
- `SmallTextRenderer` was validated pixel-for-pixel against the original V1
  Python font implementation, so the port is provably faithful rather than
  plausibly faithful.
- The simulator can run the real state machine (see ADR 0010), removing the need
  for Python re-implementations like `demo_3d_patterns.py`.

### Negative

- The dual-copy burden grows from 5 files to roughly 10. This is a real cost and
  is accepted only as an interim state — collapsing the duplication behind an
  include shim is planned as the final step of this work, gated on everything
  else being proven on hardware first.
- `ColorUtils` is a hand-port of FastLED internals. If FastLED changes its
  `hsv2rgb_rainbow` behaviour, colours drift between the firmware's own FastLED
  calls and ours. Mitigated by unit tests pinning the eight hue anchors.

## Alternatives considered

**Keep writing modes under `esp32_firmware/` and hand-port to Python for
preview.** Rejected: this is the status quo that produced `demo_3d_patterns.py`,
and it scales the drift problem linearly with each new mode.

**Move the existing six modes into `src/` as part of this change.** Rejected as
out of scope. It would mix a large refactor of working, hardware-validated code
into a feature branch. The new modules establish the pattern; migrating the old
ones is a separate piece of work.
