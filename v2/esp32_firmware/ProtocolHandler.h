#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <stdint.h>
#include <stddef.h>

struct SpriteHeader {
    uint8_t version;
    uint8_t flags;
    uint8_t width;
    uint8_t height;
    int8_t x_offset;
    int8_t y_offset;
    uint8_t rotation;
    uint8_t brightness;

    bool clearCanvas() const { return flags & 0x01; }
    bool showImmediately() const { return flags & 0x02; }
    bool marqueeActive() const { return flags & 0x04; }
};

class ProtocolHandler {
public:
    static const size_t HEADER_SIZE = 8;
    
    // Parses the header from the start of the buffer.
    // Returns true if successful.
    static bool parseHeader(const uint8_t* buffer, size_t size, SpriteHeader& header);
};

#endif
