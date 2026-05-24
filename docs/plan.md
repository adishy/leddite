# Leddite V2: Simplified 16x16 RGB Matrix API - Plan

## Objective
Refactor `leddite` into a high-performance, "Sprite-based" API for controlling a 16x16 RGB LED matrix (WS2812B) on an ESP32.

## Core Concepts
- **Canvas (16x16):** The persistent internal buffer representing the physical display.
- **Sprite:** An incoming data packet of arbitrary width/height (up to memory limits).
- **Persistence Toggle:** Each command can specify whether to clear the canvas before drawing or draw "on top" (overlay).
- **Marquee:** Support for internal automated scrolling and external frame-by-frame updates.

## Red-Green TDD Process
1. **Red:** Write a failing test for a module (e.g., `Canvas.drawSprite()`).
2. **Green:** Implement the minimum code to pass.
3. **Refactor:** Clean and optimize.

## Protocol Specification (Binary)
- **Header (Flexible):**
  - `uint8_t version`: 1 byte (for future changes)
  - `uint8_t flags`: 1 byte (Bit 0: Clear Canvas, Bit 1: Show Immediately, Bit 2: Marquee Active)
  - `uint8_t width`: 1 byte
  - `uint8_t height`: 1 byte
  - `int8_t x_offset`: 1 byte
  - `int8_t y_offset`: 1 byte
  - `uint8_t rotation`: 1 byte (0: 0°, 1: 90°, 2: 180°, 3: 270°)
  - `uint8_t brightness`: 1 byte
- **Payload:** `width * height * 3` raw RGB bytes.

## Independent Modules
1. **`Canvas`**: 16x16 buffer management, bounds checking, persistence logic.
2. **`Transformer`**: Rotation and offset calculations.
3. **`MarqueeEngine`**: Internal timer-based scrolling for sprites larger than the canvas.
4. **`ProtocolHandler`**: Packet parsing and state extraction.
5. **`LedDriver`**: FastLED integration and serpentine mapping.

## Implementation Steps

### Phase 1: Setup & Legacy Cleanup
- [ ] Move existing `leddite/`, `esp32_visual_timer/`, etc., to `legacy/`.
- [ ] Initialize `v2/` C++ project structure (src, include, test).

### Phase 2: TDD Core Logic
- [ ] **`Canvas`**: Bounds checking, clearing, sprite drawing.
- [ ] **`Transformer`**: Rotation and offset logic.
- [ ] **`ProtocolHandler`**: Binary packet parsing.

### Phase 3: Networking
- [ ] Implement WebSocket (preferred) and/or UDP listener on ESP32.
- [ ] Handle variable-length binary payloads efficiently.

### Phase 4: Hardware Integration
- [x] FastLED setup on ESP32.
- [x] Physical serpentine mapping implementation.
  - Panel is mounted 90° rotated; serpentine is **column-based right→left**.
  - `getPhysicalIndex`: even columns top→bottom `(15-x)*16+y`, odd columns bottom→top `(15-x)*16+(15-y)`.
  - Validated via dedicated diagnostic firmware (corner pixels, row/column scans, raw LED index tests).
  - See `docs/Components/HardwareMapping.md`.

### Phase 5: JS Simulator
- [x] 16x16 CSS grid UI.
- [x] Parity implementation of the `Canvas` and `Transformer` logic.
- [x] `simulator_server.py`: Integrated HTTP & WebSocket server for automatic testing.
- [x] Automatic WebSocket client in `simulator.js`.

### Phase 6: Validation & Performance
- [ ] Verify 30+ FPS full-frame updates.
- [ ] Verify multi-sprite layering (e.g., background + moving icon).

### Phase 7: API Documentation
- [x] Hardware mapping documented in `docs/Components/HardwareMapping.md`.
- [x] Firmware flashing guide in `esp32_firmware/README.md`.
- [ ] Finalize `API.md` with binary protocol details, flag definitions, and usage examples.
- [ ] Optimize for readability by other LLM agents/developers.
