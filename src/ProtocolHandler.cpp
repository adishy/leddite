#include "ProtocolHandler.h"

bool ProtocolHandler::parseHeader(const uint8_t* buffer, size_t size, SpriteHeader& header) {
    if (size < HEADER_SIZE) {
        return false;
    }

    header.version = buffer[0];
    header.flags = buffer[1];
    header.width = buffer[2];
    header.height = buffer[3];
    header.x_offset = (int8_t)buffer[4];
    header.y_offset = (int8_t)buffer[5];
    header.rotation = buffer[6];
    header.brightness = buffer[7];

    return true;
}
