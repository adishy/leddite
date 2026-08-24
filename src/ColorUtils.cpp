#include "ColorUtils.h"

// ── sin8 ──────────────────────────────────────────────────────────────────────
//
// FastLED approximates a sine wave with four quadratic segments per quarter
// wave.  Each segment is described by a base value `b` and a slope `m16`
// (scaled by 16); the table below interleaves them as (b, m16) pairs.
//
// The quarter wave is reconstructed by mirroring (bit 6 of theta) and negating
// (bit 7), so only 0..63 needs real work.

const uint8_t ColorUtils::B_M16_INTERLEAVE[8] = { 0, 49, 49, 41, 90, 27, 117, 10 };

uint8_t ColorUtils::sin8(uint8_t theta) {
    uint8_t offset = theta;

    // Bit 6 set → we're in the falling half of this half-wave; mirror the offset.
    if (theta & 0x40) {
        offset = (uint8_t)255 - offset;
    }
    offset &= 0x3F;   // 0..63

    uint8_t secoffset = offset & 0x0F;   // 0..15, position within the segment
    if (theta & 0x40) secoffset++;

    uint8_t section = (uint8_t)(offset >> 4);   // 0..3
    uint8_t s2      = (uint8_t)(section * 2);

    uint8_t b   = B_M16_INTERLEAVE[s2];
    uint8_t m16 = B_M16_INTERLEAVE[s2 + 1];

    uint8_t mx = (uint8_t)(((uint16_t)m16 * secoffset) >> 4);

    int8_t y = (int8_t)(mx + b);

    // Bit 7 set → lower half of the full wave, so negate.
    if (theta & 0x80) y = (int8_t)-y;

    return (uint8_t)(y + 128);
}

uint8_t ColorUtils::cos8(uint8_t theta) {
    return sin8((uint8_t)(theta + 64));
}

int16_t ColorUtils::sin8s(uint8_t theta) {
    return (int16_t)sin8(theta) - 128;
}

int16_t ColorUtils::cos8s(uint8_t theta) {
    return (int16_t)cos8(theta) - 128;
}

// ── scale8 ────────────────────────────────────────────────────────────────────

uint8_t ColorUtils::scale8(uint8_t i, uint8_t scale) {
    return (uint8_t)(((uint16_t)i * (uint16_t)scale) >> 8);
}

// ── hsv2rgb_rainbow ───────────────────────────────────────────────────────────
//
// Unlike a mathematically "correct" HSV conversion, FastLED's rainbow variant
// allocates hue space by perceptual distinctness and dims yellow (which the eye
// reads as ~93% as bright as white) so it doesn't stand out.  Hue is divided
// into eight 32-wide sections, selected by the top three bits.

RGB8 ColorUtils::hsv2rgb(uint8_t h, uint8_t s, uint8_t v) {
    // Yellow-correction variant selector, matching FastLED's Y1 branch.
    const uint8_t offset  = (uint8_t)(h & 0x1F);      // 0..31 within the section
    const uint8_t offset8 = (uint8_t)(offset * 8);    // 0..248

    const uint8_t third     = scale8(offset8, (256 / 3));         // offset/3
    const uint8_t twothirds = scale8(offset8, ((256 * 2) / 3));   // offset*2/3

    uint8_t r, g, b;

    if (!(h & 0x80)) {
        // Hues 0-127
        if (!(h & 0x40)) {
            if (!(h & 0x20)) {
                // 000: red → orange
                r = (uint8_t)(255 - third);
                g = third;
                b = 0;
            } else {
                // 001: orange → yellow
                r = 171;
                g = (uint8_t)(85 + third);
                b = 0;
            }
        } else {
            if (!(h & 0x20)) {
                // 010: yellow → green
                r = (uint8_t)(171 - twothirds);
                g = (uint8_t)(170 + third);
                b = 0;
            } else {
                // 011: green → aqua
                r = 0;
                g = (uint8_t)(255 - third);
                b = third;
            }
        }
    } else {
        // Hues 128-255
        if (!(h & 0x40)) {
            if (!(h & 0x20)) {
                // 100: aqua → blue
                r = 0;
                g = (uint8_t)(171 - twothirds);
                b = (uint8_t)(85 + twothirds);
            } else {
                // 101: blue → purple
                r = third;
                g = 0;
                b = (uint8_t)(255 - third);
            }
        } else {
            if (!(h & 0x20)) {
                // 110: purple → pink
                r = (uint8_t)(85 + third);
                g = 0;
                b = (uint8_t)(171 - third);
            } else {
                // 111: pink → red
                r = (uint8_t)(170 + third);
                g = 0;
                b = (uint8_t)(85 - twothirds);
            }
        }
    }

    // ── Saturation ────────────────────────────────────────────────────────────
    // Desaturating scales the colour down and adds a white floor back in, so
    // sat=0 yields pure white rather than black.
    if (s != 255) {
        if (s == 0) {
            r = 255; g = 255; b = 255;
        } else {
            const uint8_t desat   = (uint8_t)(255 - s);
            const uint8_t desat2  = scale8(desat, desat);
            const uint8_t satscale = (uint8_t)(255 - desat2);

            if (r) r = (uint8_t)(scale8(r, satscale) + 1);
            if (g) g = (uint8_t)(scale8(g, satscale) + 1);
            if (b) b = (uint8_t)(scale8(b, satscale) + 1);

            const uint8_t brightness_floor = (uint8_t)(255 - satscale);
            r = (uint8_t)(r + brightness_floor);
            g = (uint8_t)(g + brightness_floor);
            b = (uint8_t)(b + brightness_floor);
        }
    }

    // ── Value ─────────────────────────────────────────────────────────────────
    if (v != 255) {
        if (v == 0) {
            r = 0; g = 0; b = 0;
        } else {
            const uint8_t val = scale8(v, v);   // FastLED applies video-style scaling
            if (r) r = (uint8_t)(scale8(r, val) + 1);
            if (g) g = (uint8_t)(scale8(g, val) + 1);
            if (b) b = (uint8_t)(scale8(b, val) + 1);
        }
    }

    RGB8 out;
    out.r = r;
    out.g = g;
    out.b = b;
    return out;
}
