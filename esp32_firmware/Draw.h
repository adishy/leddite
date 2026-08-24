#ifndef DRAW_H
#define DRAW_H

// Draw — minimal clipped drawing primitives for the 16x16 RGB frame buffer.
//
// Pure C++/stdint only. Every mode module in src/ renders into a flat
// 16*16*3 = 768 byte buffer laid out row-major as R,G,B triples, which is
// exactly what Canvas::drawSprite() consumes and what the WASM bridge hands
// to the browser — so a buffer produced here is identical on native, WASM
// and ESP32.
//
// All primitives clip silently; callers may pass off-screen coordinates
// (which is what makes horizontal text scrolling trivial).

#include <stdint.h>

class Draw {
public:
    static const uint8_t  W    = 16;
    static const uint8_t  H    = 16;
    static const uint16_t SIZE = W * H * 3;

    static void clear(uint8_t* buf);
    static void fill(uint8_t* buf, uint8_t r, uint8_t g, uint8_t b);

    // Single pixel; out-of-range coordinates are ignored.
    static void px(uint8_t* buf, int16_t x, int16_t y,
                   uint8_t r, uint8_t g, uint8_t b);

    // Filled rectangle, clipped to the buffer.
    static void rect(uint8_t* buf, int16_t x, int16_t y, int16_t w, int16_t h,
                     uint8_t r, uint8_t g, uint8_t b);

    // Bresenham line, clipped to the buffer.
    static void line(uint8_t* buf, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     uint8_t r, uint8_t g, uint8_t b);

    // Blits an RGB sprite, skipping pure-black source pixels (treated as
    // transparent) so text and icons compose over a background.
    static void blit(uint8_t* buf, const uint8_t* src, uint16_t sw, uint16_t sh,
                     int16_t x, int16_t y);

    // As blit(), but additionally clips to rows [clipY0, clipY0 + clipH).
    // Used to keep a scrolling menu row from bleeding into its neighbours.
    static void blitClipped(uint8_t* buf, const uint8_t* src, uint16_t sw, uint16_t sh,
                            int16_t x, int16_t y, int16_t clipY0, int16_t clipH);

    // Scales a colour by 0..255 — used for dimming inactive rows.
    static uint8_t dim(uint8_t c, uint8_t scale);
};

#endif // DRAW_H
