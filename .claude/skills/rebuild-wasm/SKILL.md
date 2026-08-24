---
name: rebuild-wasm
description: Rebuild the Leddite WASM simulator module after changing src/, include/, or wasm_bridge.cpp, verify its bindings, and commit the artifact. Use whenever C++ core code changes, or when the simulator renders nothing.
---

# Rebuild the WASM module

`simulator/leddite_wasm.{js,wasm}` is the **one committed build artifact** in this
repo (`RULES.md` §1). The exception exists so Emscripten is only needed by people
changing C++ core code — and it carries an obligation:

> **Rebuild and re-commit the WASM in the same commit as any change to `src/`,
> `include/`, or `simulator/wasm_bridge.cpp`.**

A stale WASM is worse than no WASM: the simulator silently renders old logic, and
`make check-wasm` will still pass because the bindings are present.

## Steps

```bash
make simulator      # emcc build + automatic check-wasm
make test-wasm      # drive the rebuilt artifact under node
git add simulator/leddite_wasm.js simulator/leddite_wasm.wasm
```

Requires `emcc` on PATH. If it is missing, say so and stop — do **not** commit
source changes to `src/` while leaving the WASM stale.

## New bindings

Adding an embind class or method means updating the binding check in the
`check-wasm` target in the `Makefile`, which greps the `.wasm` for binding
strings. It currently requires:

```
Canvas  drawSprite  getBuffer  stopMarquee  DeviceUI  longPress  enterSettings
```

## Two failure modes this guards

Both have blacked out the simulator historically, and **native unit tests passed
through both** — they recompile `src/` and never load the artifact.

1. **Missing `wasm_bridge.cpp`.** Compiling only `src/*.cpp` yields a valid
   `.wasm` with no bindings. Emscripten reports success, `new Module.Canvas()`
   throws, and every incoming frame is dropped. `make check-wasm` catches this.

2. **Module initialisation.** Historically `index.html` needed
   `<script>var Module = {};</script>` before the glue. This is **no longer
   true** — the build uses `MODULARIZE=1` with `EXPORT_NAME=createLedditeModule`,
   so the glue exports a factory that `simulator.js` awaits. Do not reintroduce
   the global. If you change `MODULARIZE`, `simulator/index.html`,
   `simulator/simulator.js` and `simulator/test_device_ui.mjs` all need updating
   together.

## Under node

Emscripten's loader calls `fetch()` even under node and hands it a filesystem
path, failing with `ERR_INVALID_URL`. `test_device_ui.mjs` works around this by
reading the `.wasm` itself and passing `wasmBinary` to the factory. Keep that if
you touch the harness.

## Checking a build without rebuilding

```bash
make check-wasm     # scans the committed .wasm for binding strings
```
