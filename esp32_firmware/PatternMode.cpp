#include "PatternMode.h"
#include <Arduino.h>
#include <FastLED.h>
#include <string.h>

// ── Helpers ───────────────────────────────────────────────────────────────────

// Write an HSV color into pixBuf at position (x, y)
static inline void setPixHSV(uint8_t* buf, uint8_t x, uint8_t y,
                              uint8_t hue, uint8_t sat, uint8_t val) {
    CRGB rgb;
    hsv2rgb_rainbow(CHSV(hue, sat, val), rgb);
    uint32_t idx = ((uint32_t)y * 16 + x) * 3;
    buf[idx + 0] = rgb.r;
    buf[idx + 1] = rgb.g;
    buf[idx + 2] = rgb.b;
}

// Write a raw RGB color into pixBuf at position (x, y)
static inline void setPixRGB(uint8_t* buf, uint8_t x, uint8_t y,
                              uint8_t r, uint8_t g, uint8_t b) {
    uint32_t idx = ((uint32_t)y * 16 + x) * 3;
    buf[idx + 0] = r;
    buf[idx + 1] = g;
    buf[idx + 2] = b;
}

// ── Public API ────────────────────────────────────────────────────────────────

void PatternMode::begin(Canvas& canvas) {
    currentPattern = 0;
    patternStartMs = millis();
    lastFrameMs    = 0;
    memset(sparkleMap, 0, sizeof(sparkleMap));
    update(canvas);  // render first frame immediately
}

void PatternMode::nextPattern() {
    currentPattern = (currentPattern + 1) % NUM_PATTERNS;
    patternStartMs = millis();
    if (currentPattern != 3) {
        // Reset sparkle map when leaving sparkle mode
        memset(sparkleMap, 0, sizeof(sparkleMap));
    }
}

void PatternMode::update(Canvas& canvas) {
    uint32_t now = millis();

    // Auto-advance after PATTERN_DURATION_MS
    if (now - patternStartMs >= PATTERN_DURATION_MS) {
        nextPattern();
    }

    // Rate-limit to ~30 FPS
    if (now - lastFrameMs < FRAME_MS) return;
    lastFrameMs = now;

    // Draw current pattern into pixBuf
    switch (currentPattern) {
        case 0: drawRainbow();  break;
        case 1: drawLavaLamp(); break;
        case 2: drawPulse();    break;
        case 3: drawSparkle();  break;
        default: drawRainbow(); break;
    }

    // Push to canvas
    canvas.drawSprite(pixBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}

// ── Pattern implementations ───────────────────────────────────────────────────

void PatternMode::drawRainbow() {
    uint32_t t = millis() / 5;  // scroll speed: lower divisor = faster
    for (uint8_t y = 0; y < 16; y++) {
        for (uint8_t x = 0; x < 16; x++) {
            uint8_t hue = (uint8_t)((x * 16 + y * 16 + t) & 0xFF);
            setPixHSV(pixBuf, x, y, hue, 240, 160);
        }
    }
}

void PatternMode::drawLavaLamp() {
    uint32_t t = millis();

    // Two blob centers oscillating via sin8 (FastLED, 0–255 output scaled to 0–15)
    // sin8(x) returns 0–255 for a sine wave; (sin8(x) * 12 / 256) gives 0–11
    uint8_t cx1 = 2 + (sin8((uint8_t)(t / 20))  * 12 / 256);
    uint8_t cy1 = 2 + (sin8((uint8_t)(t / 17 + 100)) * 12 / 256);
    uint8_t cx2 = 2 + (sin8((uint8_t)(t / 23 + 60))  * 12 / 256);
    uint8_t cy2 = 2 + (cos8((uint8_t)(t / 19 + 150)) * 12 / 256);

    for (uint8_t y = 0; y < 16; y++) {
        for (uint8_t x = 0; x < 16; x++) {
            // Squared distance to each blob (avoid sqrt)
            int dx1 = (int)x - cx1, dy1 = (int)y - cy1;
            int dx2 = (int)x - cx2, dy2 = (int)y - cy2;
            int d1sq = dx1*dx1 + dy1*dy1;
            int d2sq = dx2*dx2 + dy2*dy2;

            // Map distance to brightness: closer = brighter (max at center)
            // Max distance squared ~ 225 (15^2 + 0^2), clamp brightness 0–200
            uint8_t b1 = (uint8_t)max(0, 200 - d1sq * 10);
            uint8_t b2 = (uint8_t)max(0, 200 - d2sq * 10);

            if (b1 >= b2) {
                // Blob 1: warm red-orange hue
                uint8_t hue = (uint8_t)(0 + (t / 100) % 30);   // slowly drift
                setPixHSV(pixBuf, x, y, hue, 255, b1 > 20 ? b1 : 0);
            } else {
                // Blob 2: cool blue-purple hue
                uint8_t hue = (uint8_t)(160 + (t / 130) % 40);
                setPixHSV(pixBuf, x, y, hue, 255, b2 > 20 ? b2 : 0);
            }
        }
    }
}

void PatternMode::drawPulse() {
    uint32_t t    = millis();
    uint8_t phase = sin8((uint8_t)(t / 10));         // 0–255 sine wave
    uint8_t bri   = (uint8_t)(40 + phase / 2);       // brightness 40–167 (low intensity)
    uint8_t hue   = (uint8_t)(160 + (t / 500) % 96); // slowly drift through blue-purple-green

    for (uint8_t y = 0; y < 16; y++) {
        for (uint8_t x = 0; x < 16; x++) {
            setPixHSV(pixBuf, x, y, hue, 220, bri);
        }
    }
}

void PatternMode::drawSparkle() {
    uint32_t t = millis();

    // Background: very dark navy
    for (uint8_t i = 0; i < 16; i++)
        for (uint8_t j = 0; j < 16; j++)
            setPixRGB(pixBuf, i, j, 0, 0, 15);

    // Add 3–5 new random sparkles this frame
    uint8_t newCount = 3 + (uint8_t)((t / 100) % 3);
    for (uint8_t i = 0; i < newCount; i++) {
        uint8_t pos = (uint8_t)random(256);
        sparkleMap[pos] = 255;
    }

    // Decay all sparkles and render
    for (uint16_t i = 0; i < 256; i++) {
        if (sparkleMap[i] > 0) {
            uint8_t bri = sparkleMap[i];
            sparkleMap[i] = (bri > 20) ? (bri - 20) : 0;
            uint8_t x = (uint8_t)(i % 16);
            uint8_t y = (uint8_t)(i / 16);
            // White sparkle over navy background
            setPixRGB(pixBuf, x, y,
                      (uint8_t)(bri),
                      (uint8_t)(bri),
                      (uint8_t)(min(255, (int)bri + 40)));
        }
    }
}
