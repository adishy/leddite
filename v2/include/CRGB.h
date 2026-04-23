#ifndef CRGB_H
#define CRGB_H

#include <stdint.h>

struct CRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool operator==(const CRGB& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

#endif
