#ifndef LedditeLEDDITE_CRGB_H
#define LedditeLEDDITE_CRGB_H

#include <stdint.h>

struct LedditeCRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool operator==(const LedditeCRGB& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

#endif
