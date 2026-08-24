# Architecture Decision Records

Significant, hard-to-reverse decisions and why they were made — including the
ones that turned out to have costs.

ADRs 0001–0008 were written retrospectively (2026-08-23) by reading the git
history; each cites the commits it is reconstructed from.

| # | Decision | Date | Status |
|---|----------|------|--------|
| [0001](0001-raspberry-pi-and-python.md) | Raspberry Pi + Python for the first Leddite | ~2024–2025 | Superseded by 0002 |
| [0002](0002-move-to-esp32-and-cpp.md) | Move to an ESP32 and C++ | 2025-06-22 | Accepted |
| [0003](0003-native-compilable-core.md) | Native-compilable C++ core, duplicated into the firmware | 2026-04-22 | Accepted |
| [0004](0004-binary-websocket-protocol.md) | 8-byte binary WebSocket protocol, not JSON | 2026-04-22 | Accepted |
| [0005](0005-wasm-browser-simulator.md) | Browser simulator powered by the real C++, via WASM | 2026-04-22 | Accepted |
| [0006](0006-column-serpentine-mapping.md) | Column-serpentine physical mapping, established empirically | 2026-05-23 | Accepted |
| [0007](0007-canvas-ack-readback.md) | Canvas ACK: pixel-exact hardware verification without a camera | 2026-05-24 | Accepted |
| [0008](0008-boot-menu-mode-dispatcher.md) | Boot menu and a mode-dispatcher firmware | 2026-05-24 | Accepted |
| [0009](0009-mode-logic-in-src-not-firmware.md) | Mode logic belongs in `src/`, not `esp32_firmware/` | 2026-08-23 | Accepted |
| [0010](0010-simulator-runs-the-real-state-machine.md) | The simulator runs the real state machine, not a copy | 2026-08-23 | Accepted |
| [0011](0011-words-not-icons-on-a-16x16-panel.md) | Words, not icons, on a 16x16 panel | 2026-08-24 | Accepted |
| [0012](0012-unlit-is-the-best-background.md) | Unlit is the best background this panel has | 2026-08-24 | Accepted |

## The thread running through these

Moving from a Raspberry Pi to an ESP32 (0002) bought hardware-timed LED output,
instant boot and a tenth of the cost — and silently traded away everything the
Python version had for free: a REPL, a virtual screen, and tests.

Most of what followed is buying that back:

- **0003** — pull rendering out of Arduino so it can be compiled and tested natively
- **0005** — compile that same code to WASM so it can be *seen* without hardware
- **0007** — have the device report its own framebuffer so hardware can be asserted on without a camera
- **0009** — extend 0003 to mode logic, which 0008 had left behind
- **0010** — extend 0005 so the simulator runs the real state machine rather than a preview of it
- **0012** — the first decision made from *rendered output* rather than from reasoning about code, which is what 0005 and 0010 were building toward all along

The recurring failure mode is **a second implementation of the same thing**:
`demo_3d_patterns.py` re-porting `PatternMode` to Python (0008), a proposed JS
renderer (0005), a proposed frame-pipe (0010). Each was rejected or regretted for
the same reason — divergence that nothing detects.
