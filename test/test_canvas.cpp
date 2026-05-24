#include "Canvas.h"
#include <iostream>
#include <cassert>

void test_initialization() {
    Canvas canvas;
    for (uint8_t y = 0; y < Canvas::HEIGHT; ++y) {
        for (uint8_t x = 0; x < Canvas::WIDTH; ++x) {
            LedditeCRGB pixel = canvas.getPixel(x, y);
            assert(pixel.r == 0 && pixel.g == 0 && pixel.b == 0);
        }
    }
    std::cout << "Test Initialization: PASSED" << std::endl;
}

void test_clear() {
    Canvas canvas;
    uint8_t data[] = { 255, 0, 0 }; // 1x1 red sprite
    canvas.drawSprite(data, 1, 1, 0, 0);
    assert(canvas.getPixel(0, 0).r == 255);
    
    canvas.clear();
    assert(canvas.getPixel(0, 0).r == 0);
    std::cout << "Test Clear: PASSED" << std::endl;
}

void test_draw_sprite() {
    Canvas canvas;
    // 2x2 green sprite
    uint8_t data[] = {
        0, 255, 0,   0, 255, 0,
        0, 255, 0,   0, 255, 0
    };
    canvas.drawSprite(data, 2, 2, 5, 5);
    
    assert(canvas.getPixel(4, 4).g == 0);
    assert(canvas.getPixel(5, 5).g == 255);
    assert(canvas.getPixel(6, 6).g == 255);
    assert(canvas.getPixel(7, 7).g == 0);
    std::cout << "Test Draw Sprite: PASSED" << std::endl;
}

void test_rotation() {
    Canvas canvas;
    // 1x2 red/green sprite
    // (0,0) = Red, (0,1) = Green
    uint8_t data[] = {
        255, 0, 0,
        0, 255, 0
    };
    // Rotate 90 deg clockwise (code 1)
    // New size 2x1. Original (0,0) -> (1,0), (0,1) -> (0,0)
    canvas.drawSprite(data, 1, 2, 0, 0, 1);
    
    assert(canvas.getPixel(1, 0).r == 255); // Original (0,0)
    assert(canvas.getPixel(0, 0).g == 255); // Original (0,1)
    std::cout << "Test Rotation: PASSED" << std::endl;
}

int main() {
    test_initialization();
    test_clear();
    test_draw_sprite();
    test_rotation();
    std::cout << "All Canvas Tests PASSED!" << std::endl;
    return 0;
}
