---
name: validate
description: Run the full Leddite validation gate — native unit tests, the WASM DeviceUI harness, WASM binding check, firmware-copy drift check, and the committed-binary check. Use before committing, before flashing, and whenever asked whether the repo is green.
---

# Leddite validation gate

Five checks. They catch **different** classes of defect, so a green `make test`
alone does not mean the repo is healthy.

```bash
make test              # 1. native unit tests
make test-wasm         # 2. committed WASM artifact, under node
make check-wasm        # 3. WASM contains its bindings
tools/sync-firmware-copies.sh --check   # 4. esp32_firmware/ copies not stale
make check-artifacts   # 5. no compiled binaries tracked in git
```

Run all five. Report any failure with its output; do not summarise a failure as
"mostly passing".

## What each one actually catches

**1. `make test`** — ~121,000 assertions across 10 binaries. The `GameEngine`
suite is a soak test over tens of thousands of steps because the real bugs it
has caught only appeared after the opening seconds of play (Invaders never
landing a shot; Dino's obstacle spawning dying after ~34 steps).

**2. `make test-wasm`** — drives the **committed** `simulator/leddite_wasm.js`
under node. This is not redundant with (1): native tests recompile `src/` and so
are blind to defects in the artifact the browser actually loads. Both times the
simulator went black historically, native tests passed throughout.

**3. `make check-wasm`** — scans the `.wasm` for its binding strings. A build
that omitted `wasm_bridge.cpp` produces a valid module with no bindings, passes
every Emscripten check, and renders nothing.

**4. `sync-firmware-copies.sh --check`** — `src/` and `esp32_firmware/` hold
duplicate copies of the core modules (the Arduino IDE cannot see `../src`). Drift
means the firmware and the tested code have diverged. Fix by running the script
without `--check`.

**5. `make check-artifacts`** — enforces `RULES.md` §1. Test binaries were once
committed as arm64 Mach-O executables, so `make test` failed with
`Exec format error` on Linux while make considered them up to date.

## If you changed `src/`, `include/`, or `simulator/wasm_bridge.cpp`

The WASM must be rebuilt and committed in the **same commit** — see the
`rebuild-wasm` skill. `make check-wasm` passing only proves the *committed* WASM
has bindings, not that it is current.

## Hardware end-to-end

`make test`/`test-wasm` never touch the device. For hardware, use the
`flash-firmware` skill, then `test_suite.py <ip> 81` — the device must be in
**Network Canvas** mode, which does **not** require physically navigating the
boot menu: use the compile-time boot-mode seam (`-DLEDDITE_BOOT_MODE=2`). See
that skill's §6. Asserting the encoder was required, without checking, wasted a
round trip once already.

Neither of these is a hardware gate. `test_suite.py` has run against real
hardware exactly once; the games have never been watched on the physical panel.
Say so rather than implying hardware coverage the repo does not have.
