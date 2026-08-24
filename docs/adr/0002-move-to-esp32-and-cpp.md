# ADR 0002 — Move to an ESP32 and C++

- **Status:** Accepted
- **Date:** 2025-06-22
- **Commit:** `4286226` "Removing python and moving PlatformIO project to top level"
- **Supersedes:** [ADR 0001](0001-raspberry-pi-and-python.md)

## Context

The Raspberry Pi build worked but was disproportionate to the job: a full Linux
computer, an SD card that degrades, a boot sequence, and a shutdown procedure —
for a device whose job is to be always-on and show pixels.

Meanwhile the ESP32 offers, on a board an order of magnitude cheaper and smaller:

- **RMT peripheral** — hardware-timed pulse generation, which is what WS2812B
  actually needs; no DMA/PWM trickery and no OS scheduler in the path
- **WiFi on-chip** — the remote-control requirement is satisfied natively
- **Instant on** — power is the on switch; pulling the plug is not a hazard
- **Deterministic timing** — a bare-metal loop rather than a preemptive OS

## Decision

Rewrite the firmware in **C++ on the ESP32** (Arduino framework, FastLED),
retiring the Python application.

The rotary encoder becomes the primary local interface, replacing the web UI for
day-to-day use.

## Consequences

**Positive.** The device boots instantly and is unkillable by power loss. LED
timing is handled in hardware. Power draw and cost drop sharply.

**Negative, and this is the big one.** Everything that was trivially testable in
Python became untestable. There is no `VirtualScreen` equivalent, no REPL, and no
way to run a screen without flashing hardware. Iteration became
*edit → compile → flash → look at the panel*.

Recovering that lost testability is what drives [ADR 0003](0003-native-compilable-core.md),
[ADR 0005](0005-wasm-browser-simulator.md), [ADR 0007](0007-canvas-ack-readback.md)
and [ADR 0009](0009-mode-logic-in-src-not-firmware.md). Each is a step back
toward what the Python version had by default.

## Alternatives considered

**Keep the Pi and optimise.** Rejected — the cost is structural, not tuning.

**A smaller Linux board.** Rejected for the same reason: any OS reintroduces boot
time, filesystem fragility and scheduler jitter.
