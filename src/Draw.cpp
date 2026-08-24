#include "Draw.h"
#include <string.h>

void Draw::clear(uint8_t* buf) {
    memset(buf, 0, SIZE);
}

void Draw::fill(uint8_t* buf, uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < W * H; i++) {
        buf[i * 3]     = r;
        buf[i * 3 + 1] = g;
        buf[i * 3 + 2] = b;
    }
}

void Draw::px(uint8_t* buf, int16_t x, int16_t y,
              uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= (int16_t)W || y < 0 || y >= (int16_t)H) return;
    const uint16_t i = (uint16_t)((y * W + x) * 3);
    buf[i]     = r;
    buf[i + 1] = g;
    buf[i + 2] = b;
}

void Draw::rect(uint8_t* buf, int16_t x, int16_t y, int16_t w, int16_t h,
                uint8_t r, uint8_t g, uint8_t b) {
    for (int16_t yy = y; yy < y + h; yy++)
        for (int16_t xx = x; xx < x + w; xx++)
            px(buf, xx, yy, r, g, b);
}

void Draw::line(uint8_t* buf, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                uint8_t r, uint8_t g, uint8_t b) {
    int16_t dx =  (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    for (;;) {
        px(buf, x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        const int16_t e2 = (int16_t)(2 * err);
        if (e2 >= dy) { err = (int16_t)(err + dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <= dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

void Draw::blit(uint8_t* buf, const uint8_t* src, uint16_t sw, uint16_t sh,
                int16_t x, int16_t y) {
    blitClipped(buf, src, sw, sh, x, y, 0, (int16_t)H);
}

void Draw::blitClipped(uint8_t* buf, const uint8_t* src, uint16_t sw, uint16_t sh,
                       int16_t x, int16_t y, int16_t clipY0, int16_t clipH) {
    for (uint16_t sy = 0; sy < sh; sy++) {
        const int16_t dy = (int16_t)(y + sy);
        if (dy < clipY0 || dy >= clipY0 + clipH) continue;

        for (uint16_t sx = 0; sx < sw; sx++) {
            const uint32_t si = ((uint32_t)sy * sw + sx) * 3;
            const uint8_t  r = src[si], g = src[si + 1], b = src[si + 2];

            // Pure black is transparent, so glyphs compose over a highlight band.
            if ((r | g | b) == 0) continue;

            px(buf, (int16_t)(x + sx), dy, r, g, b);
        }
    }
}

void Draw::stencilClipped(uint8_t* buf, const uint8_t* src, uint16_t sw, uint16_t sh,
                          int16_t x, int16_t y, int16_t clipY0, int16_t clipH,
                          uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t sy = 0; sy < sh; sy++) {
        const int16_t dy = (int16_t)(y + sy);
        if (dy < clipY0 || dy >= clipY0 + clipH) continue;

        for (uint16_t sx = 0; sx < sw; sx++) {
            const uint32_t si = ((uint32_t)sy * sw + sx) * 3;

            // Coverage only — the source colour is discarded. A black *output*
            // is therefore still written, which is the whole point.
            if ((src[si] | src[si + 1] | src[si + 2]) == 0) continue;

            px(buf, (int16_t)(x + sx), dy, r, g, b);
        }
    }
}

uint8_t Draw::dim(uint8_t c, uint8_t scale) {
    return (uint8_t)(((uint16_t)c * (uint16_t)scale) >> 8);
}
