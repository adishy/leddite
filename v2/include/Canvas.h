#ifndef CANVAS_H
#define CANVAS_H

#include "CRGB.h"
#include <stdint.h>

class Canvas {
public:
    static const uint8_t WIDTH = 16;
    static const uint8_t HEIGHT = 16;

    Canvas();
    void clear();
    void drawSprite(const uint8_t* data, uint8_t w, uint8_t h, int8_t x, int8_t y, uint8_t rotation = 0, bool clearBefore = false);
    CRGB getPixel(uint8_t x, uint8_t y) const;
    const CRGB* getBuffer() const;

private:
    CRGB buffer[WIDTH * HEIGHT];
};

#endif
