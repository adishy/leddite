# Leddite V2: High-Performance RGB Matrix API

Leddite V2 is a streamlined, "Sprite-based" binary API for controlling 16x16 RGB LED matrices (WS2812B) on an ESP32. 

## Key Features
- **100% Logic Parity:** The C++ core is compiled to WebAssembly (WASM) for the JS Simulator, ensuring that digital tests perfectly predict hardware behavior.
- **Binary Protocol:** Optimized 8-byte header + raw RGB payload for low-latency, high-frequency updates (30+ FPS).
- **Sprite Architecture:** Supports both full-frame 16x16 updates and small, variable-sized icons or text strips.
- **Automated Marquee:** Internal C++ engine for smooth, sub-pixel-perfect scrolling of large sprites.
- **TDD-First:** Every core module (`Canvas`, `Transformer`, `ProtocolHandler`, `MarqueeEngine`) is verified with native C++ unit tests.

## Quick Start
1.  **Run the Simulator:**
    ```bash
    make run-sim
    ```
    Open `http://localhost:8000` to see the WASM-powered grid in action.
2.  **Run Unit Tests:**
    ```bash
    make test
    ```
3.  **Flash ESP32:**
    Upload `v2/esp32_firmware/esp32_firmware.ino` using the Arduino IDE (requires FastLED and WebSockets libraries).

## Project Structure
- `v2/src`, `v2/include`: Core C++ logic.
- `v2/esp32_firmware`: Production firmware for the ESP32.
- `v2/simulator`: WASM-powered browser simulator.
- `docs/`: Obsidian vault containing fact sheets for all components.
- `legacy/`: Archive of the original Python/C++ V1 implementation.

## Binary API Details
See [v2/API.md](v2/API.md) for the full protocol specification.
