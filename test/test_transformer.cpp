#include "Transformer.h"
#include <iostream>
#include <cassert>

void test_90_deg() {
    uint8_t out_x, out_y, out_w, out_h;
    // 2x3 sprite rotated 90 deg (code 1) should be 3x2
    Transformer::transformCoords(0, 0, 2, 3, 1, out_x, out_y, out_w, out_h);
    assert(out_w == 3 && out_h == 2);
    assert(out_x == 2 && out_y == 0);

    Transformer::transformCoords(1, 2, 2, 3, 1, out_x, out_y, out_w, out_h);
    assert(out_x == 0 && out_y == 1);
    
    std::cout << "Test 90 Deg Rotation: PASSED" << std::endl;
}

void test_180_deg() {
    uint8_t out_x, out_y, out_w, out_h;
    // (x, y) -> (w-1-x, h-1-y) (code 2)
    Transformer::transformCoords(0, 0, 2, 3, 2, out_x, out_y, out_w, out_h);
    assert(out_w == 2 && out_h == 3);
    assert(out_x == 1 && out_y == 2);

    std::cout << "Test 180 Deg Rotation: PASSED" << std::endl;
}

int main() {
    test_90_deg();
    test_180_deg();
    std::cout << "All Transformer Tests PASSED!" << std::endl;
    return 0;
}
