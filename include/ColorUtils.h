#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

// ColorUtils — native ports of the FastLED math the modes rely on.
//
// Pure C++/stdint only — no Arduino/FastLED headers. Compiles natively for unit
// tests, under Emscripten for the simulator, and on the ESP32.
//
// WHY THIS EXISTS
// ---------------
// Mode code used to call FastLED's sin8/cos8/hsv2rgb_rainbow directly, which
// pinned it to the Arduino build and made it impossible to unit-test or render
// in the simulator.  These are faithful ports of the FastLED algorithms, so a
// pattern rendered natively, in WASM, and on hardware produces byte-identical
// output — which is what lets the unit tests assert on real pixel values.
//
// Ported from FastLED's lib8tion (sin8_C, scale8) and hsv2rgb_rainbow.

#include <stdint.h>

struct RGB8 {
    uint8_t r, g, b;
};

class ColorUtils {
public:
    // ── Trig ──────────────────────────────────────────────────────────────────
    // theta 0-255 maps to 0-360 degrees; output 0-255 with 128 as the midpoint.
    // Piecewise-quadratic approximation, bit-exact with FastLED's sin8_C.
    static uint8_t sin8(uint8_t theta);
    static uint8_t cos8(uint8_t theta);   // == sin8(theta + 64)

    // Signed convenience wrappers: result in [-128, 127].
    // Equivalent to (int16_t)sin8(theta) - 128, the idiom the FastLED-derived
    // animation code uses for anything that oscillates around an origin.
    static int16_t sin8s(uint8_t theta);
    static int16_t cos8s(uint8_t theta);

    // ── Scaling ───────────────────────────────────────────────────────────────
    // (i * scale) / 256 — FastLED's scale8.
    static uint8_t scale8(uint8_t i, uint8_t scale);

    // ── Colour conversion ─────────────────────────────────────────────────────
    // FastLED's "rainbow" HSV, which spends more of the hue range on the colours
    // the eye distinguishes best and brightness-corrects yellow. h/s/v are 0-255.
    static RGB8 hsv2rgb(uint8_t h, uint8_t s, uint8_t v);

private:
    // Interleaved (b, m16) pairs for the four quarter-wave sections of sin8.
    static const uint8_t B_M16_INTERLEAVE[8];
};

#endif // COLOR_UTILS_H
