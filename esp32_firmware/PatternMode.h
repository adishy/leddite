#pragma once

#include "Canvas.h"
#include <stdint.h>

// PatternMode — self-contained pattern slideshow (no network required).
//
// Four patterns, cycling automatically every PATTERN_DURATION_MS (15 s),
// or immediately on encoder turn via nextPattern().
//
// Patterns:
//   0: Zoom Cube   — infinite square-tunnel zooming toward viewer (early-2000s OS vibe)
//   1: Spin Pyramid — rotating wireframe pyramid, isometric projection
//   2: Sparkle      — random twinkling pixels on dark background (unchanged)
//   3: Orb Chase    — two colour orbs orbiting at different speeds (circles chasing circles)
//
// All patterns render at ~30 FPS (gated by lastFrameMs).
// pixBuf is a member to avoid 768-byte stack allocation in each draw call.
class PatternMode {
public:
    static const uint32_t PATTERN_DURATION_MS = 15000;
    static const uint8_t  NUM_PATTERNS        = 4;
    static const uint8_t  FRAME_MS            = 33;   // ~30 FPS

    void begin(Canvas& canvas);
    void update(Canvas& canvas);   // call each loop()
    void nextPattern();            // advance immediately (encoder turn or press)

private:
    void drawZoomCube();     // 0: infinite square tunnel
    void drawSpinPyramid();  // 1: rotating wireframe pyramid
    void drawSparkle();      // 2: twinkling pixels
    void drawOrbChase();     // 3: two orbiting colour blobs

    // Bresenham line into pixBuf (clips to 16×16)
    static void drawLine(uint8_t* buf,
                         int8_t x0, int8_t y0, int8_t x1, int8_t y1,
                         uint8_t r, uint8_t g, uint8_t b);

    uint8_t  pixBuf[16 * 16 * 3];  // 768 B — kept as member to avoid stack pressure
    uint8_t  sparkleMap[256];       // per-pixel brightness for sparkle decay
    uint8_t  currentPattern = 0;
    uint32_t patternStartMs = 0;
    uint32_t lastFrameMs    = 0;
};
