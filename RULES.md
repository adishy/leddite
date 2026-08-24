# Repository Rules

Short, enforceable rules for this repo. Agents: these are binding — see also
`CLAUDE.md`, which points here.

---

## 1. Never commit build artifacts

**Compiled output does not belong in git.** If a file is produced by running a
command in this repo, it is an artifact, and artifacts are ignored, not tracked.

This is not a style preference. Committed binaries actively broke the build:
`test/test_canvas` and friends were committed as **arm64 Mach-O** executables, so
on any Linux machine `make test` failed with:

```
/bin/sh: 1: ./test/test_canvas: Exec format error
```

Make considered the stale binaries up to date and never rebuilt them. Anyone on a
different platform than the last committer got a broken test suite with a
confusing error. They entered history through `git add -A` after a build, and
`.gitignore` had no `test/` entries to stop it.

### Enforcement

```bash
make check-artifacts     # fails if a tracked file is a compiled binary
```

Run it before committing, or install it as a hook:

```bash
git config core.hooksPath .githooks
```

### The one exception: `simulator/leddite_wasm.{js,wasm}`

These **are** committed, deliberately. The reason is documented in `CLAUDE.md`:
it means Emscripten is only needed by people changing C++ core code, and everyone
else can run the simulator straight from a clone.

The exception carries an obligation. A stale WASM is worse than no WASM — it has
blacked out the simulator twice (see the WASM pitfalls section in `CLAUDE.md`).
So:

- **Rebuild and re-commit the WASM in the same commit as any change to `src/`,
  `include/`, or `simulator/wasm_bridge.cpp`.**
- `make check-wasm` verifies a committed WASM actually contains its bindings.
- Never hand-edit `leddite_wasm.js`.

If the no-Emscripten-needed property ever stops being worth it, delete the
exception and the two files — nothing else depends on them being tracked.

### What is *not* an artifact

`hardware_build/**.svg`, `hardware_build/**.pdf` and the datasheet are binary but
are **sources** — nothing in this repo generates them. They stay tracked.

---

## 2. Editing core C++ means editing it twice

Until the dual-copy is collapsed (tracked as the final step of the
game-screensavers work), files existing in both `src/` and `esp32_firmware/` must
be changed in both places, and the WASM rebuilt. See `docs/adr/0009` for why the
duplication exists and what replaces it.

---

## 3. New mode logic goes in `src/`, not `esp32_firmware/`

Arduino-free, with time and randomness injected, so it can be unit-tested and run
in the simulator. Full rationale in `docs/adr/0009-mode-logic-in-src-not-firmware.md`.
