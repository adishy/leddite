# ADR 0006 — Column-serpentine physical mapping, established empirically

- **Status:** Accepted
- **Date:** 2026-05-23
- **Commits:** `f9ae68b` "mapping diagnostic — step-by-step pattern verification", `14670c8` "ground-truth solid fill test using legacy proven mapping"

## Context

A 16×16 WS2812B panel is a single 256-pixel chain folded into a grid. **How** it
is folded is a property of the physical panel, not something the software may
choose. Get it wrong and output is scrambled in a way that looks like a rendering
bug — mirrored, transposed, or striped — sending debugging in the wrong direction.

Panels vary: row-major, column-major, boustrophedon in either axis, and any
starting corner.

## Decision

Determine the mapping **experimentally**, then encode it in exactly one function.

The diagnostic sketches lit single pixels and rows in sequence and recorded what
the panel actually did. The answer for this hardware: **column-serpentine — even
columns top→bottom, odd columns bottom→top, columns ordered right→left**:

```cpp
uint16_t getPhysicalIndex(uint8_t x, uint8_t y) {
    if (x % 2 == 0) return (15 - x) * 16 + y;
    else            return (15 - x) * 16 + (15 - y);
}
```

This is the **only** place in the codebase that knows about physical layout. Every
other component — `Canvas`, the protocol, the modes, the simulator — works in
logical `(x, y)` with `(0,0)` at top-left.

## Consequences

**Positive.** The simulator does not need to model the wiring at all, which is
why WASM output can be compared directly against logical expectations. Rendering
bugs and wiring bugs cannot be confused.

**Negative.** The constant is hardware-specific and undiscoverable from the code
alone; a differently-wired panel needs the diagnostic rerun. It is documented in
`docs/Components/HardwareMapping.md`.

## Note

The mapping was cross-checked against the pre-existing `esp32_visual_timer`
sketch, which had a working display — "the legacy proven mapping" in `14670c8`.
When empirical results and a known-good implementation agree, the answer is
probably right.
