#pragma once

#include "Canvas.h"
#include <stdint.h>

// PatternMode — self-contained pattern slideshow (no network required).
//
// Four patterns, cycling automatically every PATTERN_DURATION_MS (15s),
// or immediately on encoder turn via nextPattern().
//
// Patterns:
//   0: Rainbow Wave — HSV diagonal rainbow scrolling
//   1: Lava Lamp    — two slow-moving color blobs via sine oscillation
//   2: Pulse        — full canvas breathing in a single hue
//   3: Sparkle      — random twinkling pixels on dark background
//
// All patterns render at ~30 FPS (gated by lastFrameMs).
// pixBuf is a member to avoid 768-byte stack allocation in each draw call.
class PatternMode {
public:
    static const uint32_t PATTERN_DURATION_MS = 15000;
    static const uint8_t  NUM_PATTERNS        = 4;
    static const uint8_t  FRAME_MS            = 33;  // ~30 FPS

    void begin(Canvas& canvas);
    void update(Canvas& canvas);    // call each loop()
    void nextPattern();             // advance immediately (encoder turn)

private:
    void drawRainbow();
    void drawLavaLamp();
    void drawPulse();
    void drawSparkle();

    uint8_t  pixBuf[16 * 16 * 3];   // 768 bytes — member to avoid stack pressure
    uint8_t  sparkleMap[256];        // per-pixel brightness for sparkle decay
    uint8_t  currentPattern  = 0;
    uint32_t patternStartMs  = 0;
    uint32_t lastFrameMs     = 0;
};
