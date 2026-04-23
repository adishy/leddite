#include "Transformer.h"

void Transformer::transformCoords(uint8_t x, uint8_t y, uint8_t w, uint8_t h, 
                                uint8_t rotation, 
                                uint8_t& out_x, uint8_t& out_y, 
                                uint8_t& out_w, uint8_t& out_h) {
    switch (rotation) {
        case 1: // 90 deg
            out_x = h - 1 - y;
            out_y = x;
            out_w = h;
            out_h = w;
            break;
        case 2: // 180 deg
            out_x = w - 1 - x;
            out_y = h - 1 - y;
            out_w = w;
            out_h = h;
            break;
        case 3: // 270 deg
            out_x = y;
            out_y = w - 1 - x;
            out_w = h;
            out_h = w;
            break;
        case 0: // 0 deg
        default:
            out_x = x;
            out_y = y;
            out_w = w;
            out_h = h;
            break;
    }
}
