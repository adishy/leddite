# ADR 0008 — Boot menu and a mode-dispatcher firmware

- **Status:** Accepted
- **Date:** 2026-05-24
- **Commits:** `c4a059e` "boot menu + rotary encoder + 4 modes", `0e49abf` "full name marquee in menu, 24h clock, screen off on menu long-press"

## Context

Until this point the firmware did exactly one thing: run a WebSocket server and
render what it was sent. Useful, but it meant the device was a dumb panel — with
no laptop connected it displayed nothing, and adding a clock would have meant a
separate build.

## Decision

Restructure the firmware as a **mode dispatcher** with a boot menu.

`AppMode` (`AppState.h`) enumerates the modes. `loop()` switches on the current
mode and delegates. Each mode is a self-contained class with `begin(canvas)` and
`update(canvas)`, plus optional `onEncoderTurn` / `onEncoderPress`.

The rotary encoder becomes the universal interface:

- **Turn** — navigate / adjust
- **Short press** — select / confirm
- **Long press (3 s)** — universal "back" gesture, from any mode to the menu
- **Long press in the menu** — screen off; short press wakes

The menu shows the full mode name scrolled through `MarqueeEngine` with
indicator dots, rather than two-letter codes.

## Consequences

**Positive.** The device is useful standalone. Adding a mode is a self-contained
class plus a dispatch arm. The encoder grammar is consistent enough to be learned
once, and Network Canvas becomes one mode among several rather than the whole
firmware.

**Negative — and this is the flaw that mattered.** Every mode class was written
directly against `Arduino.h` and `FastLED`, and placed only in
`esp32_firmware/`. So the whole category of "mode" sits **outside** the
native-compilable core that [ADR 0003](0003-native-compilable-core.md)
established: no unit tests, no simulator, back to *flash and squint*.

The cost surfaced concretely with `demo_3d_patterns.py` — a hand-written third
implementation of `PatternMode` in Python, existing purely so the patterns could
be previewed, with nothing keeping it in sync.

[ADR 0009](0009-mode-logic-in-src-not-firmware.md) reverses this for new modes.

**Also negative:** the universal long-press was defined as "return to menu" with
no notion of hierarchy, which had to be revisited once modes gained submenus.
