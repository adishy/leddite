#include "ProtocolHandler.h"
#include <iostream>
#include <cassert>

void test_header_parse() {
    // 1x1 green sprite at (5, -5), rotate 90, clear canvas, show now.
    // Flags: Bit 0 (1), Bit 1 (2) = 3
    uint8_t buffer[] = {
        1,      // version
        3,      // flags
        16,     // width
        16,     // height
        5,      // x_offset
        (uint8_t)-5,     // y_offset
        1,      // rotation (90)
        128     // brightness
    };

    SpriteHeader header;
    bool success = ProtocolHandler::parseHeader(buffer, sizeof(buffer), header);
    
    assert(success);
    assert(header.version == 1);
    assert(header.clearCanvas() == true);
    assert(header.showImmediately() == true);
    assert(header.width == 16);
    assert(header.height == 16);
    assert(header.x_offset == 5);
    assert(header.y_offset == -5);
    assert(header.rotation == 1);
    assert(header.brightness == 128);

    std::cout << "Test Header Parse: PASSED" << std::endl;
}

void test_invalid_size() {
    uint8_t buffer[] = { 1, 2, 3 }; // Too short
    SpriteHeader header;
    bool success = ProtocolHandler::parseHeader(buffer, sizeof(buffer), header);
    assert(!success);
    std::cout << "Test Invalid Size: PASSED" << std::endl;
}

int main() {
    test_header_parse();
    test_invalid_size();
    std::cout << "All Protocol Tests PASSED!" << std::endl;
    return 0;
}
