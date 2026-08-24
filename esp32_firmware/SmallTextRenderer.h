#ifndef SMALL_TEXT_RENDERER_H
#define SMALL_TEXT_RENDERER_H

// SmallTextRenderer — 3x4 proportional bitmap font renderer
// Pure C++/stdint only — no Arduino/ESP32 headers. Compiles natively for unit tests.
//
// Ported from Leddite V1's `FontSmall` ("terminal_clear_small"), recovered from
// legacy/leddite/hw/fonts/font_small.py at commit 65ea60f^.
//
// WHY A SECOND FONT
// -----------------
// TextRenderer's glyphs are 5x7, which fits only two text rows on a 16x16 panel.
// These are 4px tall, so three rows plus a position indicator fit — which is what
// makes a real vertical submenu possible.
//
// ENCODING
// --------
// Each character is 4 bytes, one per column, LSB = topmost row (same convention
// as TextRenderer).  Only the low 4 bits of each byte are used.
//
// PROPORTIONAL ADVANCE ("dynamic kerning")
// ----------------------------------------
// V1 rendered a glyph by dropping every all-blank column and then appending
// `kerning` (1) blank columns.  That is reproduced exactly here, so advances are:
//   letters, digits, '-', '/'  4px      ':'  '.'  '*'  3px
//   '1'                        3px      '!'       2px      ' '  1px
// A 16px row therefore holds 4 characters without scrolling.
//
// Supports: 0-9, A-Z, space, '!', '.', ':', '/', '-', and '*' (degree mark).
// Unrecognised characters render as spaces.

#include <stdint.h>
#include <stddef.h>

class SmallTextRenderer {
public:
    static const uint8_t CHAR_HEIGHT = 4;   // glyph pixel height
    static const uint8_t MAX_WIDTH   = 4;   // columns stored per glyph
    static const uint8_t KERNING     = 1;   // blank columns appended per glyph

    // The font has no ASCII degree sign, so '*' is designated as the degree mark
    // (a 2x2 block in the top-left of the cell). Use it as e.g. "12*C".
    static const char DEGREE_CHAR = '*';

    // Renders text into outPixels (caller allocates >= bufferSize(text) bytes).
    // outW = textWidth(text), outH = CHAR_HEIGHT.
    // color: pointer to [r, g, b].
    static void renderText(const char* text, uint8_t* outPixels,
                           uint16_t& outW, uint16_t& outH,
                           const uint8_t* color);

    // Total advance width in pixels, including each glyph's trailing kern column.
    static uint16_t textWidth(const char* text);

    // Advance width of a single character, including its trailing kern column.
    static uint8_t charWidth(char c);

    // Required pixel buffer size in bytes: textWidth(text) * CHAR_HEIGHT * 3.
    static uint32_t bufferSize(const char* text);

    // Font data — public so unit tests can verify glyph values directly.
    // Indexed by (ASCII - FONT_MIN_CHAR); supported range 0x20 (' ') to 0x5A ('Z').
    static const uint8_t FONT_DATA[][MAX_WIDTH];
    static const uint8_t FONT_MIN_CHAR = 0x20;   // ' '
    static const uint8_t FONT_MAX_CHAR = 0x5A;   // 'Z'

private:
    // Returns pointer to the 4-byte column data for a character,
    // or the space glyph if unsupported.
    static const uint8_t* getGlyph(char c);

    // Number of non-blank columns in a glyph (the proportional part of advance).
    static uint8_t glyphInkWidth(const uint8_t* glyph);
};

#endif // SMALL_TEXT_RENDERER_H
