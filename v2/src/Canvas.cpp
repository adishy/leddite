#include "Canvas.h"
#include "Transformer.h"
#include <string.h>

Canvas::Canvas() {
    clear();
}

void Canvas::clear() {
    memset(buffer, 0, sizeof(buffer));
}

void Canvas::drawSprite(const uint8_t* data, uint8_t w, uint8_t h, int8_t x_offset, int8_t y_offset, uint8_t rotation, bool clearBefore) {
    if (clearBefore) {
        clear();
    }

    for (uint8_t sy = 0; sy < h; ++sy) {
        for (uint8_t sx = 0; sx < w; ++sx) {
            uint8_t tx, ty, tw, th;
            Transformer::transformCoords(sx, sy, w, h, rotation, tx, ty, tw, th);

            int8_t px = tx + x_offset;
            int8_t py = ty + y_offset;

            if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT) continue;

            uint16_t spriteIdx = (sy * w + sx) * 3;
            uint16_t canvasIdx = py * WIDTH + px;

            buffer[canvasIdx].r = data[spriteIdx];
            buffer[canvasIdx].g = data[spriteIdx + 1];
            buffer[canvasIdx].b = data[spriteIdx + 2];
        }
    }
}

CRGB Canvas::getPixel(uint8_t x, uint8_t y) const {
    if (x >= WIDTH || y >= HEIGHT) {
        return {0, 0, 0};
    }
    return buffer[y * WIDTH + x];
}

const CRGB* Canvas::getBuffer() const {
    return buffer;
}
