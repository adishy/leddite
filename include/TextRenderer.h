#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

// TextRenderer — 5×7 bitmap font renderer
// Pure C++/stdint only — no Arduino/ESP32 headers. Compiles natively for unit tests.
//
// Font encoding: each character is 5 bytes (one per column), LSB = topmost row.
// Character stride = 6px (5px glyph + 1px gap), matching leddite_client.py FONT dict.
//
// Supports: 0–9, A–Z, space, '!', '?', ':', '-', '.'

#include <stdint.h>
#include <stddef.h>

class TextRenderer {
public:
    static const uint8_t CHAR_WIDTH  = 5;   // glyph pixel width
    static const uint8_t CHAR_HEIGHT = 7;   // glyph pixel height
    static const uint8_t CHAR_STRIDE = 6;   // pixels per character slot (includes 1px gap)

    // Renders text into outPixels buffer (caller allocates).
    // outW = strlen(text) * CHAR_STRIDE, outH = CHAR_HEIGHT (7)
    // color: pointer to [r, g, b] bytes (3 bytes)
    // Unrecognised characters are rendered as spaces.
    static void renderText(const char* text, uint8_t* outPixels,
                           uint16_t& outW, uint16_t& outH,
                           const uint8_t* color);

    // Returns the pixel width for a given text string: strlen(text) * CHAR_STRIDE
    static uint16_t textWidth(const char* text);

    // Returns the required pixel buffer size in bytes for a text string
    static uint32_t bufferSize(const char* text);

    // Font data — public so unit tests can verify glyph values directly
    // Indexed by ASCII value; supported range 0x20 (' ') to 0x5A ('Z')
    static const uint8_t FONT_DATA[][5];
    static const uint8_t FONT_MIN_CHAR = 0x20; // ' '
    static const uint8_t FONT_MAX_CHAR = 0x5A; // 'Z'

private:
    // Returns pointer to 5-byte font column data for a character, or space glyph if unsupported
    static const uint8_t* getGlyph(char c);
};

#endif // TEXT_RENDERER_H
