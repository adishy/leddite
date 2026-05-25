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
    memset(sparkleMap, 0, sizeof(sparkleMap));
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
        case 0: drawSpinningCube(); break;
        case 1: drawSpiral();       break;
        case 2: drawSparkle();      break;
        case 3: drawOrbChase();     break;
        default: drawSpinningCube(); break;
    }

    canvas.drawSprite(pixBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}

// ── Pattern 0: Spinning Cube ──────────────────────────────────────────────────
//
// A wireframe cube runs through a 9-second cycle, then repeats in a new colour:
//
//   Phase 1 (0-3 s): Y-axis (horizontal) spin only.  A 42-unit fixed tilt on
//     the X axis keeps depth visible; otherwise a straight-on view looks flat.
//   Phase 2 (3-6 s): X-axis spin joins in, both axes rotating together.
//   Phase 3 (6-9 s): scale grows from 2 → 8, cube fills then engulfs the screen.
//
// Vertex encoding: bit 0 = x sign, bit 1 = y sign, bit 2 = z sign.
//   Vertices 0-3 (bit2=0, z=-s) form the back face  → drawn dim.
//   Vertices 4-7 (bit2=1, z=+s) form the front face → drawn bright.
//   Lateral edges 0-4, 1-5, 2-6, 3-7 connect them   → drawn medium.
//
// Painter's order (back → laterals → front) ensures front edges overdraw back
// edges where they cross, giving an approximate depth sort with no extra cost.
//
// Trig via FastLED sin8/cos8: 0-255 maps to 0-360°; (sin8(a)-128) ∈ [-128,127].
// After multiplying two int8 values the result fits comfortably in int16.

void PatternMode::drawSpinningCube() {
    memset(pixBuf, 0, sizeof(pixBuf));

    uint32_t t = millis();

    // 9-second colour cycle: 6 hues (42 apart) across rainbow
    const uint32_t CYCLE_MS = 9000;
    uint32_t phase_ms  = t % CYCLE_MS;
    uint8_t  cycle_idx = (uint8_t)((t / CYCLE_MS) % 6);
    uint8_t  hue       = cycle_idx * 42;

    // Continuous base angles — never freeze/jump at phase boundaries
    uint8_t ang_y = (uint8_t)(t / 20);   // full Y rotation ≈ 5.1 s
    uint8_t ang_x;
    uint8_t s;

    if (phase_ms < 3000) {
        ang_x = 42;                        // fixed tilt (~60°) — shows depth without X spin
        s = 5;
    } else if (phase_ms < 6000) {
        ang_x = (uint8_t)(t / 30);        // X starts rotating; Y continues
        s = 5;
    } else {
        ang_x = (uint8_t)(t / 30);
        uint32_t z = phase_ms - 6000;
        s = (uint8_t)(2 + z * 7 / 3000);  // 2 → 9 over 3 s
        if (s > 9) s = 9;
    }

    // Precompute trig (range -128..127 after subtracting 128)
    int16_t cy = (int16_t)(cos8(ang_y) - 128);
    int16_t sy = (int16_t)(sin8(ang_y) - 128);
    int16_t cx = (int16_t)(cos8(ang_x) - 128);
    int16_t sx = (int16_t)(sin8(ang_x) - 128);

    // Project 8 vertices onto screen
    int8_t px[8], py[8];
    for (uint8_t i = 0; i < 8; i++) {
        int8_t vx = (i & 1) ? (int8_t)s : -(int8_t)s;
        int8_t vy = (i & 2) ? (int8_t)s : -(int8_t)s;
        int8_t vz = (i & 4) ? (int8_t)s : -(int8_t)s;

        // Y rotation (horizontal spin around vertical axis)
        int16_t rx = ((int16_t)vx * cy - (int16_t)vz * sy) >> 7;
        int16_t rz = ((int16_t)vx * sy + (int16_t)vz * cy) >> 7;

        // X rotation (vertical spin around horizontal axis)
        int16_t ry2 = ((int16_t)vy * cx - rz * sx) >> 7;

        px[i] = (int8_t)(8 + (int8_t)rx);
        py[i] = (int8_t)(8 + (int8_t)ry2);
    }

    // Depth-shaded colours: back face dim, lateral medium, front face bright
    CRGB bright, mid, dim_col;
    hsv2rgb_rainbow(CHSV(hue, 220, 220), bright);
    hsv2rgb_rainbow(CHSV(hue, 200, 140), mid);
    hsv2rgb_rainbow(CHSV(hue, 220,  70), dim_col);

    // Back face (vertices 0-3, z=-s) — draw first so front edges overdraw
    drawLine(pixBuf, px[0], py[0], px[1], py[1], dim_col.r, dim_col.g, dim_col.b);
    drawLine(pixBuf, px[1], py[1], px[3], py[3], dim_col.r, dim_col.g, dim_col.b);
    drawLine(pixBuf, px[3], py[3], px[2], py[2], dim_col.r, dim_col.g, dim_col.b);
    drawLine(pixBuf, px[2], py[2], px[0], py[0], dim_col.r, dim_col.g, dim_col.b);

    // Lateral edges (back → front)
    drawLine(pixBuf, px[0], py[0], px[4], py[4], mid.r, mid.g, mid.b);
    drawLine(pixBuf, px[1], py[1], px[5], py[5], mid.r, mid.g, mid.b);
    drawLine(pixBuf, px[2], py[2], px[6], py[6], mid.r, mid.g, mid.b);
    drawLine(pixBuf, px[3], py[3], px[7], py[7], mid.r, mid.g, mid.b);

    // Front face (vertices 4-7, z=+s) — drawn last, on top
    drawLine(pixBuf, px[4], py[4], px[5], py[5], bright.r, bright.g, bright.b);
    drawLine(pixBuf, px[5], py[5], px[7], py[7], bright.r, bright.g, bright.b);
    drawLine(pixBuf, px[7], py[7], px[6], py[6], bright.r, bright.g, bright.b);
    drawLine(pixBuf, px[6], py[6], px[4], py[4], bright.r, bright.g, bright.b);
}

// ── Pattern 1: Spiral ─────────────────────────────────────────────────────────
//
// Single Archimedean spiral arm that grows outward from the centre, then resets.
//
// The arm has up to N=8 segments (radii 1-7 px).  A "grow counter" advances
// every GROW_STEP_MS milliseconds, extending the visible arm by one segment.
// When all 8 segments are visible the arm holds for HOLD_MS then resets.
//
// Meanwhile the whole arm rotates continuously: t_rot = (uint8_t)(t/16),
// one full rotation every ≈ 4.1 s, so each growth cycle starts at a fresh angle.
//
// ANGLE_STEP = 55 sin8-units ≈ 77° per segment → 7 × 77° ≈ 540° total (1.5 turns).
//
// Colour: hue drifts slowly; along the arm the hue shifts +9 per segment and
// brightness ramps from 60 (dim at centre) to 255 (bright at tip).

void PatternMode::drawSpiral() {
    memset(pixBuf, 0, sizeof(pixBuf));

    uint32_t t       = millis();
    uint8_t  t_rot   = (uint8_t)(t / 16);   // one full rotation ≈ 4.1 s
    uint8_t  baseHue = (uint8_t)(t / 31);   // hue drift ≈ 8 s per full cycle

    const uint8_t  N            = 8;
    const uint8_t  ANGLE_STEP   = 55;
    const uint32_t GROW_STEP_MS = 400;   // time per new segment (8 segs × 400 ms = 3.2 s)
    const uint32_t HOLD_MS      = 600;   // linger at full size before reset
    const uint32_t CYCLE_MS     = (uint32_t)N * GROW_STEP_MS + HOLD_MS;  // 3800 ms

    uint32_t t_cycle   = t % CYCLE_MS;
    uint8_t  n_visible = (t_cycle < (uint32_t)N * GROW_STEP_MS)
                         ? (uint8_t)(t_cycle / GROW_STEP_MS + 1)   // 1..N
                         : N;                                        // hold at full

    int8_t prevX = 8, prevY = 8;
    for (uint8_t i = 1; i <= n_visible; i++) {
        uint8_t theta = t_rot + (uint8_t)(i * ANGLE_STEP);
        uint8_t r     = i;   // 1..7 px from centre

        int8_t x = (int8_t)(8 + (((int16_t)(cos8(theta) - 128) * (int16_t)r) >> 7));
        int8_t y = (int8_t)(8 + (((int16_t)(sin8(theta) - 128) * (int16_t)r) >> 7));

        uint8_t bri = (uint8_t)(60 + (uint16_t)195 * i / N);   // 60→255 along arm
        uint8_t hue = baseHue + (uint8_t)(i * 9);              // hue gradient
        CRGB col;
        hsv2rgb_rainbow(CHSV(hue, 230, bri), col);

        drawLine(pixBuf, prevX, prevY, x, y, col.r, col.g, col.b);
        prevX = x;
        prevY = y;
    }
}

// ── Pattern 2: Sparkle ────────────────────────────────────────────────────────

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

// ── Pattern 3: Orb Chase ──────────────────────────────────────────────────────

void PatternMode::drawOrbChase() {
    uint32_t t = millis();

    uint8_t a1  = (uint8_t)(t / 28);
    int8_t  o1x = 8 + (int8_t)(((int16_t)(cos8(a1) - 128) * 5) >> 7);
    int8_t  o1y = 8 + (int8_t)(((int16_t)(sin8(a1) - 128) * 5) >> 7);

    uint8_t a2  = (uint8_t)(t / 15);
    int8_t  o2x = 8 + (int8_t)(((int16_t)(cos8(a2) - 128) * 3) >> 7);
    int8_t  o2y = 8 + (int8_t)(((int16_t)(sin8(a2) - 128) * 3) >> 7);

    uint8_t hue1 = (uint8_t)(160 + (t / 900) % 40);
    uint8_t hue2 = (uint8_t)(220 + (t / 700) % 30);

    for (int8_t y = 0; y < 16; y++) {
        for (int8_t x = 0; x < 16; x++) {
            int16_t dx1 = x - o1x, dy1 = y - o1y;
            int16_t dx2 = x - o2x, dy2 = y - o2y;
            int16_t d1sq = dx1*dx1 + dy1*dy1;
            int16_t d2sq = dx2*dx2 + dy2*dy2;

            uint8_t b1 = (d1sq < 30) ? (uint8_t)max(0, 220 - (int16_t)(d1sq * 12)) : 0;
            uint8_t b2 = (d2sq < 30) ? (uint8_t)max(0, 220 - (int16_t)(d2sq * 12)) : 0;

            if (b1 == 0 && b2 == 0) {
                setPixRGB(pixBuf, (uint8_t)x, (uint8_t)y, 0, 0, 8);
            } else if (b1 >= b2) {
                setPixHSV(pixBuf, (uint8_t)x, (uint8_t)y, hue1, 240, b1);
            } else {
                setPixHSV(pixBuf, (uint8_t)x, (uint8_t)y, hue2, 240, b2);
            }
        }
    }
}
