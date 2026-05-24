#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include <stdint.h>

class Transformer {
public:
    // Transforms coordinates (x, y) of a sprite of size (w, h) 
    // into (new_x, new_y) after a given rotation (0, 90, 180, 270).
    // Also returns the new width and height.
    static void transformCoords(uint8_t x, uint8_t y, uint8_t w, uint8_t h, 
                                uint8_t rotation, 
                                uint8_t& out_x, uint8_t& out_y, 
                                uint8_t& out_w, uint8_t& out_h);
};

#endif
