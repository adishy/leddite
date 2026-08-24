# ADR 0003 — A native-compilable C++ core, duplicated into the firmware

- **Status:** Accepted (the duplication is under review — see [ADR 0009](0009-mode-logic-in-src-not-firmware.md))
- **Date:** 2026-04-22
- **Commit:** `a1624a9` "Implement Canvas, Transformer, ProtocolHandler logic with unit tests and JS Simulator"

## Context

[ADR 0002](0002-move-to-esp32-and-cpp.md) bought hardware-timed LED output at the
cost of all testability. Every bug — a rotation off by one, a sprite clipped on
the wrong edge — could only be found by flashing and squinting at a 16×16 panel.

The rendering logic itself has no reason to be hardware-bound. Compositing a
sprite into a 16×16 buffer, rotating it, clipping it, and scrolling a marquee are
pure functions over bytes.

## Decision

Split that logic into a **pure C++ core with no Arduino dependencies**, living in
`src/` + `include/`:

`Canvas`, `Transformer`, `MarqueeEngine`, `ProtocolHandler`, `TextRenderer`

It compiles three ways from one source: `g++` for unit tests, `emcc` for the
browser simulator ([ADR 0005](0005-wasm-browser-simulator.md)), and `arduino-cli`
for the device.

### The duplication

The Arduino IDE compiles only files sitting beside the `.ino` and cannot reach
`../src`. So each core file exists **twice** — once in `src/`, once in
`esp32_firmware/` — and both copies must be kept in step.

This is a genuine wart, accepted deliberately: the alternative was giving up
native tests, and the copies are byte-identical so the sync is mechanical.

## Consequences

**Positive.** Rendering became unit-testable, and the same bytes are produced on
every target.

**Negative.** Two copies drift silently if someone edits one. Initially this was
guarded only by a note in `CLAUDE.md`; it is now mechanised by
`tools/sync-firmware-copies.sh` (with `--check` for CI) and stated as a rule in
`RULES.md` §2. Collapsing the duplication entirely remains open work.

## Alternatives considered

**Symlinks from `esp32_firmware/` into `src/`.** Not reliably followed by the
Arduino builder, and hostile to Windows checkouts.

**A PlatformIO `lib_dir` pointing at `src/`.** Would work, but the project builds
with `arduino-cli` and the IDE, and moving the whole build system was out of
scope for the change that introduced the split.
