# ADR 0005 — Browser simulator powered by the real C++, via WASM

- **Status:** Accepted, extended by [ADR 0010](0010-simulator-runs-the-real-state-machine.md)
- **Date:** 2026-04-22
- **Commits:** `f4183a6` "Adding separate simulator", `ec1bd4f` "Add Makefile for unit tests and WASM simulator build"

## Context

[ADR 0003](0003-native-compilable-core.md) made rendering unit-testable, but unit
tests assert on bytes. Nobody can look at an array of 768 integers and say "the
marquee is scrolling too fast" or "that glyph is clipped". Some things only
judge by eye.

Rewriting the renderer in JavaScript for a web preview would create a second
implementation whose divergence from the firmware would be invisible — the
preview would look right while the device did something else.

## Decision

Compile the **same `src/` C++ to WebAssembly** with Emscripten and render it in
the browser. `simulator/wasm_bridge.cpp` exposes `Canvas` via `embind`;
`simulator.js` reads the pixel buffer straight out of the WASM heap and paints a
16×16 grid of divs.

`simulator_server.py` runs an HTTP server for the static files and a WebSocket
relay on 8765, so the same client scripts drive either the simulator or the
device by changing a port.

**The compiled `.js` and `.wasm` are committed.** This is the single exception to
the no-build-artifacts rule (`RULES.md` §1), so that Emscripten is only needed by
people changing C++ core code and everyone else can run the simulator from a
clone.

## Consequences

**Positive.** Pixel-exact preview with no second implementation. The iteration
loop for rendering work drops from *flash and squint* to a browser refresh.

**Negative — and this bit twice.** A committed artifact can go stale or be built
wrong, and the failure is silent:

1. Compiling `src/*.cpp` **without** `wasm_bridge.cpp` yields a valid `.wasm`
   with no bindings. Emscripten reports success, `new Module.Canvas()` throws,
   and every frame is dropped. The display is simply black.
2. Emscripten 3.x needed `var Module = {}` declared before the glue script or
   `onRuntimeInitialized` never fired. Also silently black.

Native unit tests passed throughout both, because they rebuild `src/` and never
load the artifact. `make check-wasm` was added to scan the binary for its binding
strings. Pitfall (2) was later removed structurally by switching to
`MODULARIZE=1` — see [ADR 0010](0010-simulator-runs-the-real-state-machine.md).

## Alternatives considered

**A JS reimplementation of the renderer.** Rejected: two implementations, silent
divergence. This is the same reasoning that later rejected a Python frame-pipe in
ADR 0010.

**Screenshots from hardware.** Rejected: needs the hardware, and a camera cannot
give pixel-exact answers.
