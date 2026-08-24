# ADR 0010 — The simulator runs the real state machine, not a copy of it

- **Status:** Accepted
- **Date:** 2026-08-23
- **Supersedes part of:** the original plan for this branch
- **Builds on:** [ADR 0009](0009-mode-logic-in-src-not-firmware.md)
- **Extends:** [ADR 0005](0005-wasm-browser-simulator.md) from rendering to the full UI

## Context

ADR 0009 moved mode logic into `src/` so it could be unit-tested. That left an
open question: how do the new screens get *seen* before hardware?

The initial plan was a native `tools/frame_dump.cpp` that rendered game frames to
stdout, plus a `preview_games.py` that wrapped them in the binary protocol and
sent them over WebSocket to the simulator. `test_suite.py` would then assert on
`/canvas-state`.

This was challenged on the grounds that the codebase already has a
write-once-run-everywhere mechanism: C++ compiled to WASM and driven from JS. The
challenge was correct. The frame-pipe would have been a *second* delivery path
for the same pixels, and it only existed to fit game output into a harness
(`test_suite.py` / `/canvas-state`) built for the **binary protocol** — which the
games do not use and have no reason to.

There is also a specific failure mode this repo has hit twice, documented under
"WASM pitfalls" in `CLAUDE.md`:

1. a `.wasm` compiled without `wasm_bridge.cpp` — valid, passes every Emscripten
   check, contains no bindings, simulator silently black;
2. a missing `var Module = {}` pre-declaration — `onRuntimeInitialized` never
   fires, simulator silently black.

Native unit tests passed throughout both incidents. They compile `src/` afresh
and never touch the artifact the browser loads.

## Decision

**One delivery path: the browser runs the actual C++ state machine via WASM.**

- `src/UiController` holds the whole Games/Settings/Weather navigation state
  machine — screens, input handling, cycling, persisted values. It is Arduino-free
  per ADR 0009.
- `simulator/wasm_bridge.cpp` exposes it as a `DeviceUI` embind class
  (`turn` / `press` / `longPress` / `tick` / `getBuffer`, plus getters).
- The ESP32 wrappers (`GameMode`, `SettingsMode`) drive **the same
  `UiController`**. The simulator is therefore not a model of the firmware; it is
  the firmware's own logic with a different shell around it.
- `tools/frame_dump.cpp` and `preview_games.py` are not built. `test_suite.py`
  stays scoped to the binary protocol.

**Automated coverage of the artifact:** `simulator/test_device_ui.mjs` loads the
**committed** `leddite_wasm.js` under node and drives `DeviceUI` through the full
tree, asserting on `getBuffer()`. Run via `make test-wasm`.

**Build change:** the WASM is now built with `MODULARIZE=1` and
`EXPORT_NAME=createLedditeModule`. This was required for node to load the glue,
and it also **eliminates pitfall (2) entirely** — there is no global `Module` to
forget to pre-declare, because the glue exports a factory that callers await.
`index.html` no longer carries the `var Module = {}` line.

`make check-wasm` was extended to require `DeviceUI`, `longPress` and
`enterSettings` in the binary, so pitfall (1) now also covers the new bindings.

## Consequences

### Positive

- No third implementation. `demo_3d_patterns.py` exists because the old
  architecture forced a Python re-port; nothing here repeats that.
- The class of bug that blacked out the simulator twice is now caught
  automatically, by tests that load the real file rather than rebuilding sources.
- The submenu, brightness editor, place picker, unit toggle and weather view are
  all drivable in the browser with the existing encoder buttons and keys, before
  any hardware is flashed.
- Phase 5 shrinks: the firmware wrappers hold almost no logic.

### Negative

- `MODULARIZE=1` changes the simulator's init contract. Anything loading
  `leddite_wasm.js` and expecting a global `Module` breaks; `simulator.js` was
  updated, and nothing else in the repo did so.
- Under node, Emscripten's loader reaches for `fetch()` and hands it a filesystem
  path, failing with `ERR_INVALID_URL`. The harness works around this by reading
  the `.wasm` itself and passing `wasmBinary` to the factory. The browser path is
  unaffected.
- `UiController` now owns state the firmware must mirror into NVS. It exposes
  plain getters plus a `settingsDirty()` flag rather than knowing about storage.

## Alternatives considered

**Keep the frame-pipe for `test_suite.py` assertions.** Rejected: it duplicates
the delivery path and bends a protocol-testing harness around non-protocol
features. Native unit tests already assert on game pixels directly, faster and
with more precision than a WebSocket round-trip.

**Reimplement the menus in JavaScript for the simulator.** Rejected outright —
this is the exact drift ADR 0009 was written to stop.
