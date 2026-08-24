# ADR 0007 — Canvas ACK: pixel-exact hardware verification without a camera

- **Status:** Accepted
- **Date:** 2026-05-24
- **Commits:** `5352016` "pixel-level canvas assertions for agent-usable testing", `71b3b56` "FLAG_ACK_CANVAS — pixel-exact hardware verification without camera"

## Context

The simulator ([ADR 0005](0005-wasm-browser-simulator.md)) proves the *logic* is
right. It cannot prove the **device** is right — that the packet survived WiFi,
that `Canvas` composited it as expected on real hardware, that the panel is
actually lit.

The obvious answers are all bad:

- **Point a camera at it.** Needs a rig, lighting, and vision; gives approximate
  colours and cannot distinguish "pixel is off" from "pixel is dim".
- **Trust the serial log.** Only proves a packet arrived, not what was rendered.
- **Look at it.** Requires a human in the loop for every assertion, so nothing is
  automatable and nothing can run in CI or be driven by an agent.

## Decision

Make the device **report its own framebuffer back**. Flag bit `0x08` on an
incoming packet means "reply with a canvas ACK", and the receiver immediately
sends:

```
[0xCA, 16, 16, r,g,b × 256]   = 771 bytes
```

The same flag works against the simulator, so one test asserts identically on
both targets.

## Consequences

**Positive.** Hardware tests make **exact** pixel assertions with no camera, no
vision model and no human. `test_suite.py` runs unchanged against
`localhost:8765` or `<esp32-ip>:81`. This is what makes hardware verification
agent-drivable at all.

**Negative.** Costs a flag bit (four of eight now used). It reports the logical
`Canvas` buffer, so it verifies everything up to but **not including**
`getPhysicalIndex` and the LEDs themselves — a rewiring fault or a dead pixel is
invisible to it. That boundary is deliberate and worth remembering: a green ACK
test plus a dark panel means the fault is in the mapping or the hardware, and
[ADR 0006](0006-column-serpentine-mapping.md) is where to look.

## Alternatives considered

**A separate HTTP readback endpoint.** Rejected — a second transport and a second
code path; the flag reuses the connection already open.

**Always ACK.** Rejected: doubles traffic for animation, where nobody is asserting.
