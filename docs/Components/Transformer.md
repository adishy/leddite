# Transformer

**Role:** Handles coordinate transformations for clockwise rotation of sprites.

## Key Facts
- **Rotation Codes:** 
    - `0`: 0° (Normal)
    - `1`: 90° (Clockwise)
    - `2`: 180° (Clockwise)
    - `3`: 270° (Clockwise)
- **Logic:** `transformCoords(x, y, w, h, rotation)` outputs the new `(x, y, w, h)`.
- **Implementation:** Pure C++ logic, no external dependencies.
- **WASM Parity:** Compiled into the simulator to ensure digital tests perfectly predict hardware behavior.

## Behavior
- Swaps `width` and `height` for 90° and 270° rotations.
- Always rotates *before* applying `(x, y)` offsets.
