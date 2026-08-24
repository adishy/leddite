# ADR 0001 — Raspberry Pi + Python for the first Leddite

- **Status:** Superseded by [ADR 0002](0002-move-to-esp32-and-cpp.md)
- **Date:** ~2024–2025 (pre-dates the current history; reconstructed from `legacy/`)
- **Evidence:** `git show 65ea60f^:legacy/leddite/` — removed in `65ea60f`

## Context

The goal was a 16×16 WS2812B panel that could show a clock, calendar, weather and
arbitrary images, and be driven remotely.

WS2812B needs a tightly-timed 800 kHz single-wire signal. A general-purpose OS
cannot bit-bang that reliably, so the choice of host is constrained by what can
generate the waveform: on a Raspberry Pi that means DMA + PWM on GPIO 18, which
is what `rpi_ws281x` provides.

## Decision

Run the whole system as a **Python application on a Raspberry Pi**.

- `rpi_ws281x` / `Adafruit_NeoPixel` for the panel (GPIO 18, DMA channel 10,
  800 kHz)
- A Flask web app (`leddite/api/`, `leddite/views/`) for remote control, with a
  browser pixel editor
- A "context" abstraction (`leddite/hw/contexts/`) for each screen — clock,
  calendar, weather, IP, heartbeat, welcome — cycled by a carousel
- `Screen` / `PhysicalScreen` / `VirtualScreen` so scenes could be developed
  without hardware
- Bitmap fonts in `leddite/hw/fonts/` (`FontSmall` 3×4, `FontMed` 5×7) with a
  `Track`/`Scene` layout engine that scrolled a row only when its content
  exceeded the screen width
- Deployed as a systemd unit (`hw_setup/leddite.service`)

## Consequences

**Positive.** Fast to build in. A full HTTP API, templating and image handling
came essentially free. `VirtualScreen` meant scenes were developable off-device.

**Negative.** A Raspberry Pi is a large, power-hungry, slow-booting computer to
dedicate to a picture frame; it needs an SD card that wears out, and it wants a
clean shutdown. The Python process competes with the OS scheduler for the timing
the LED signal needs.

## Legacy that outlived it

The V1 fonts and the `Track`/`Scene` "scroll only if wider than the screen" model
were recovered from `65ea60f^` and ported to C++ for the vertical submenus —
see [ADR 0009](0009-mode-logic-in-src-not-firmware.md). Deleting code is not the
same as losing it; the git history was the design archive.
