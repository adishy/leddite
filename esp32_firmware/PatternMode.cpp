#include "PatternMode.h"
#include <Arduino.h>
#include <FastLED.h>
#include <string.h>

// ── Pixel helpers ─────────────────────────────────────────────────────────────

static inline void setPixHSV(uint8_t* buf, uint8_t x, uint8_t y,
                              uint8_t hue, uint8_t sat, uint8_t val) {
    CRGB rgb;
    hsv2rgb_rainbow(CHSV(hue, sat, val), rgb);
    uint32_t idx = ((uint32_t)y * 16 + x) * 3;
    buf[idx]     = rgb.r;
    buf[idx + 1] = rgb.g;
    buf[idx + 2] = rgb.b;
}

static inline void setPixRGB(uint8_t* buf, uint8_t x, uint8_t y,
                              uint8_t r, uint8_t g, uint8_t b) {
    uint32_t idx = ((uint32_t)y * 16 + x) * 3;
    buf[idx]     = r;
    buf[idx + 1] = g;
    buf[idx + 2] = b;
}

// ── Bresenham line ────────────────────────────────────────────────────────────

void PatternMode::drawLine(uint8_t* buf,
                           int8_t x0, int8_t y0, int8_t x1, int8_t y1,
                           uint8_t r, uint8_t g, uint8_t b) {
    int8_t dx =  (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int8_t dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int8_t sx = (x0 < x1) ? 1 : -1;
    int8_t sy = (y0 < y1) ? 1 : -1;
    int8_t err = dx + dy;

    for (;;) {
        if (x0 >= 0 && x0 < 16 && y0 >= 0 && y0 < 16) {
            uint16_t i = ((uint16_t)(uint8_t)y0 * 16 + (uint8_t)x0) * 3;
            buf[i]     = r;
            buf[i + 1] = g;
            buf[i + 2] = b;
        }
        if (x0 == x1 && y0 == y1) break;
        int8_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void PatternMode::begin(Canvas& canvas) {
    currentPattern = 0;
    patternStartMs = millis();
    lastFrameMs    = 0;
    memset(sparkleMap, 0, sizeof(sparkleMap));
    update(canvas);   // render first frame immediately
}

void PatternMode::nextPattern() {
    currentPattern = (currentPattern + 1) % NUM_PATTERNS;
    patternStartMs = millis();
    if (currentPattern != 2) {
        memset(sparkleMap, 0, sizeof(sparkleMap));
    }
}

void PatternMode::update(Canvas& canvas) {
    uint32_t now = millis();

    // Auto-advance
    if (now - patternStartMs >= PATTERN_DURATION_MS) {
        nextPattern();
    }

    // Rate-limit to ~30 FPS
    if (now - lastFrameMs < FRAME_MS) return;
    lastFrameMs = now;

    switch (currentPattern) {
        case 0: drawZoomCube();    break;
        case 1: drawSpinPyramid(); break;
        case 2: drawSparkle();     break;
        case 3: drawOrbChase();    break;
        default: drawZoomCube();   break;
    }

    canvas.drawSprite(pixBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}

// ── Pattern 0: Zoom Cube ──────────────────────────────────────────────────────
//
// Five wireframe square rings expand from the screen centre outward, creating
// the illusion of flying into an infinite cube tunnel.  Each ring starts small
// and dim (far away) and grows bright as it fills the screen (close).
// Hue drifts slowly through deep blue → violet → cyan.
//
// Phase arithmetic: base_phase (0-255) advances with time.  Each of the 5
// rings is staggered by 51 (≈255/5) so they stay evenly distributed in depth
// at all times.  phase >> 5 maps 0-255 to sizes 0-7; +1 avoids a dot at centre.

void PatternMode::drawZoomCube() {
    memset(pixBuf, 0, sizeof(pixBuf));

    uint32_t t = millis();
    uint8_t  basePhase = (uint8_t)(t / 30);   // full cycle ≈ 7.7 s

    for (uint8_t ring = 0; ring < 5; ring++) {
        uint8_t phase = basePhase + ring * 51; // stagger (wraps naturally in uint8_t)
        uint8_t s     = 1 + (phase >> 5);      // half-size 1–8

        // Brightness: small (far) = dim, large (close) = bright
        uint8_t bri = 40 + (uint8_t)(s * 27);  // 67 – 256 → clamp to 255
        if (bri > 255) bri = 255;

        // Hue: blue-violet range, each ring offset by +12
        uint8_t hue = (uint8_t)(160 + (t / 400) + ring * 12);

        CRGB rgb;
        hsv2rgb_rainbow(CHSV(hue, 230, bri), rgb);

        // Square outline centred at (7,7) with half-size s
        //   x: [8-s .. 7+s]    y: [8-s .. 7+s]
        int8_t x0 = (int8_t)(8 - s), x1 = (int8_t)(7 + s);
        int8_t y0 = (int8_t)(8 - s), y1 = (int8_t)(7 + s);

        drawLine(pixBuf, x0, y0, x1, y0, rgb.r, rgb.g, rgb.b);  // top
        drawLine(pixBuf, x0, y1, x1, y1, rgb.r, rgb.g, rgb.b);  // bottom
        drawLine(pixBuf, x0, y0, x0, y1, rgb.r, rgb.g, rgb.b);  // left
        drawLine(pixBuf, x1, y0, x1, y1, rgb.r, rgb.g, rgb.b);  // right
    }
}

// ── Pattern 1: Spin Pyramid ───────────────────────────────────────────────────
//
// Wireframe square pyramid, viewed from slightly above, rotating around the
// vertical axis.  Uses an isometric projection of the base (which spins in the
// X-Z plane) with a fixed apex above screen centre.
//
// Projection (orthographic isometric from above-right):
//   screen_x = cx  +  (x3d - z3d) * 7/8
//   screen_y = cy  +  (x3d + z3d) / 2
//
// Base uses FastLED sin8/cos8 (0-255 range, 128 = zero) for integer trig.
// Lateral edges (apex→corner) are bright; base edges are dim for depth cue.

void PatternMode::drawSpinPyramid() {
    memset(pixBuf, 0, sizeof(pixBuf));

    uint32_t t  = millis();
    uint8_t  th = (uint8_t)(t / 20);   // full rotation ≈ 5.1 s

    // Apex: fixed, mapped from 3D (0, -5, 0) → screen (8, 3)
    const int8_t AX = 8, AY = 3;

    // Base centre on screen, pushed slightly below midpoint
    const int8_t CX = 8, CY = 10;
    const uint8_t BSCALE = 4;   // base radius in 3D units

    int8_t bx[4], by[4];
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t angle = th + i * 64;   // four corners, 90° apart
        // Fixed-point: (sin8(a) - 128) * BSCALE / 128
        int8_t x3d = (int8_t)(((int16_t)(cos8(angle) - 128) * BSCALE) >> 7);
        int8_t z3d = (int8_t)(((int16_t)(sin8(angle) - 128) * BSCALE) >> 7);
        // Isometric projection
        bx[i] = CX + (int8_t)(((int16_t)(x3d - z3d) * 7) >> 3);
        by[i] = CY + (int8_t)((x3d + z3d) / 2);
    }

    // Colour: teal-cyan, drifts slowly
    uint8_t hue = (uint8_t)(128 + (t / 700) % 40);
    CRGB bright, dim;
    hsv2rgb_rainbow(CHSV(hue, 210, 210), bright);
    hsv2rgb_rainbow(CHSV(hue, 210,  80), dim);

    // Lateral edges: apex → each base corner (bright)
    for (uint8_t i = 0; i < 4; i++) {
        drawLine(pixBuf, AX, AY, bx[i], by[i], bright.r, bright.g, bright.b);
    }

    // Base edges (dim — depth cue)
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t j = (i + 1) & 3;
        drawLine(pixBuf, bx[i], by[i], bx[j], by[j], dim.r, dim.g, dim.b);
    }
}

// ── Pattern 2: Sparkle ────────────────────────────────────────────────────────
// (unchanged from original)

void PatternMode::drawSparkle() {
    uint32_t t = millis();

    // Dark navy background
    for (uint8_t i = 0; i < 16; i++)
        for (uint8_t j = 0; j < 16; j++)
            setPixRGB(pixBuf, i, j, 0, 0, 15);

    // Spawn 3-5 new sparkles per frame
    uint8_t newCount = 3 + (uint8_t)((t / 100) % 3);
    for (uint8_t i = 0; i < newCount; i++) {
        uint8_t pos = (uint8_t)random(256);
        sparkleMap[pos] = 255;
    }

    // Decay and render
    for (uint16_t i = 0; i < 256; i++) {
        if (sparkleMap[i] > 0) {
            uint8_t bri = sparkleMap[i];
            sparkleMap[i] = (bri > 20) ? (bri - 20) : 0;
            uint8_t x = (uint8_t)(i % 16);
            uint8_t y = (uint8_t)(i / 16);
            setPixRGB(pixBuf, x, y,
                      bri, bri,
                      (uint8_t)(bri < 215 ? bri + 40 : 255));
        }
    }
}

// ── Pattern 3: Orb Chase (circles chasing circles) ────────────────────────────
//
// Two colour orbs orbit the screen centre at different speeds and radii.
// The outer orb (blue-violet, r=5) moves slower; the inner orb (magenta, r=3)
// moves faster and "chases" the outer one.  Each orb has a soft radial glow.

void PatternMode::drawOrbChase() {
    uint32_t t = millis();

    // Orb 1 — outer orbit, slower (blue-violet)
    uint8_t a1  = (uint8_t)(t / 28);
    int8_t  o1x = 8 + (int8_t)(((int16_t)(cos8(a1) - 128) * 5) >> 7);
    int8_t  o1y = 8 + (int8_t)(((int16_t)(sin8(a1) - 128) * 5) >> 7);

    // Orb 2 — inner orbit, faster (magenta-rose)
    uint8_t a2  = (uint8_t)(t / 15);
    int8_t  o2x = 8 + (int8_t)(((int16_t)(cos8(a2) - 128) * 3) >> 7);
    int8_t  o2y = 8 + (int8_t)(((int16_t)(sin8(a2) - 128) * 3) >> 7);

    // Hues drift slowly
    uint8_t hue1 = (uint8_t)(160 + (t / 900) % 40);   // blue → violet
    uint8_t hue2 = (uint8_t)(220 + (t / 700) % 30);   // magenta → rose

    for (int8_t y = 0; y < 16; y++) {
        for (int8_t x = 0; x < 16; x++) {
            int16_t dx1 = x - o1x, dy1 = y - o1y;
            int16_t dx2 = x - o2x, dy2 = y - o2y;
            int16_t d1sq = dx1*dx1 + dy1*dy1;
            int16_t d2sq = dx2*dx2 + dy2*dy2;

            // Glow falls off quadratically; threshold at d²=30 to stay dark outside
            uint8_t b1 = (d1sq < 30) ? (uint8_t)max(0, 220 - (int16_t)(d1sq * 12)) : 0;
            uint8_t b2 = (d2sq < 30) ? (uint8_t)max(0, 220 - (int16_t)(d2sq * 12)) : 0;

            if (b1 == 0 && b2 == 0) {
                setPixRGB(pixBuf, (uint8_t)x, (uint8_t)y, 0, 0, 8); // near-black bg
            } else if (b1 >= b2) {
                setPixHSV(pixBuf, (uint8_t)x, (uint8_t)y, hue1, 240, b1);
            } else {
                setPixHSV(pixBuf, (uint8_t)x, (uint8_t)y, hue2, 240, b2);
            }
        }
    }
}
