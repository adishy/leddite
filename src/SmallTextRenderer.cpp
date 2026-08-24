#include "SmallTextRenderer.h"
#include <string.h>

// ── Font data ─────────────────────────────────────────────────────────────────
//
// Generated from legacy/leddite/hw/fonts/font_small.py (commit 65ea60f^) rather
// than transcribed by hand.  Four bytes per character, one per column,
// LSB = topmost row, low 4 bits used.
//
// '-' and '*' (degree mark) are additions — V1's FontSmall had neither, and
// sub-zero temperatures need a minus sign.

const uint8_t SmallTextRenderer::FONT_DATA[][SmallTextRenderer::MAX_WIDTH] = {
    {0x00, 0x00, 0x00, 0x00},  // 0x20  ' '
    {0x0B, 0x00, 0x00, 0x00},  // 0x21  '!'
    {0x00, 0x00, 0x00, 0x00},  // 0x22  '"'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x23  '#'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x24  '$'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x25  '%'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x26  '&'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x27  '\''  unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x28  '('   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x29  ')'   unsupported
    {0x03, 0x03, 0x00, 0x00},  // 0x2A  '*'   degree mark (2x2, top-left)
    {0x00, 0x00, 0x00, 0x00},  // 0x2B  '+'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x2C  ','   unsupported
    {0x04, 0x04, 0x04, 0x00},  // 0x2D  '-'
    {0x0C, 0x0C, 0x00, 0x00},  // 0x2E  '.'
    {0x08, 0x0E, 0x01, 0x00},  // 0x2F  '/'
    {0x0F, 0x09, 0x0F, 0x00},  // 0x30  '0'
    {0x02, 0x0F, 0x00, 0x00},  // 0x31  '1'
    {0x0D, 0x0D, 0x0B, 0x00},  // 0x32  '2'
    {0x09, 0x0F, 0x0F, 0x00},  // 0x33  '3'
    {0x06, 0x07, 0x0F, 0x00},  // 0x34  '4'
    {0x0B, 0x0D, 0x0D, 0x00},  // 0x35  '5'
    {0x0F, 0x0D, 0x0D, 0x00},  // 0x36  '6'
    {0x01, 0x01, 0x0F, 0x00},  // 0x37  '7'
    {0x0F, 0x0D, 0x0F, 0x00},  // 0x38  '8'
    {0x07, 0x05, 0x0F, 0x00},  // 0x39  '9'
    {0x0A, 0x0A, 0x00, 0x00},  // 0x3A  ':'
    {0x00, 0x00, 0x00, 0x00},  // 0x3B  ';'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x3C  '<'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x3D  '='   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x3E  '>'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x3F  '?'   unsupported
    {0x00, 0x00, 0x00, 0x00},  // 0x40  '@'   unsupported
    {0x0F, 0x05, 0x0F, 0x00},  // 0x41  'A'
    {0x0F, 0x0A, 0x0E, 0x00},  // 0x42  'B'
    {0x0F, 0x09, 0x09, 0x00},  // 0x43  'C'
    {0x0E, 0x0A, 0x0F, 0x00},  // 0x44  'D'
    {0x06, 0x0B, 0x0B, 0x00},  // 0x45  'E'
    {0x0F, 0x05, 0x01, 0x00},  // 0x46  'F'
    {0x0F, 0x09, 0x0D, 0x00},  // 0x47  'G'
    {0x0F, 0x02, 0x0E, 0x00},  // 0x48  'H'
    {0x09, 0x0F, 0x09, 0x00},  // 0x49  'I'
    {0x0C, 0x08, 0x0F, 0x00},  // 0x4A  'J'
    {0x0F, 0x06, 0x09, 0x00},  // 0x4B  'K'
    {0x0F, 0x08, 0x08, 0x00},  // 0x4C  'L'
    {0x0F, 0x03, 0x0F, 0x00},  // 0x4D  'M'
    {0x0F, 0x01, 0x0E, 0x00},  // 0x4E  'N'
    {0x0F, 0x09, 0x0F, 0x00},  // 0x4F  'O'
    {0x0F, 0x05, 0x07, 0x00},  // 0x50  'P'
    {0x07, 0x05, 0x0F, 0x00},  // 0x51  'Q'
    {0x0F, 0x07, 0x0B, 0x00},  // 0x52  'R'
    {0x0B, 0x09, 0x0D, 0x00},  // 0x53  'S'
    {0x01, 0x0F, 0x01, 0x00},  // 0x54  'T'
    {0x0F, 0x08, 0x0F, 0x00},  // 0x55  'U'
    {0x07, 0x08, 0x07, 0x00},  // 0x56  'V'
    {0x0F, 0x0C, 0x0F, 0x00},  // 0x57  'W'
    {0x09, 0x06, 0x09, 0x00},  // 0x58  'X'
    {0x03, 0x0E, 0x03, 0x00},  // 0x59  'Y'
    {0x0D, 0x0D, 0x0B, 0x00},  // 0x5A  'Z'
};

// ── Private helpers ───────────────────────────────────────────────────────────

const uint8_t* SmallTextRenderer::getGlyph(char c) {
    uint8_t uc = (uint8_t)c;

    // Fold lowercase to uppercase — the font has no lowercase forms.
    if (uc >= 'a' && uc <= 'z') uc = (uint8_t)(uc - 'a' + 'A');

    if (uc < FONT_MIN_CHAR || uc > FONT_MAX_CHAR) {
        return FONT_DATA[0];   // space
    }
    return FONT_DATA[uc - FONT_MIN_CHAR];
}

uint8_t SmallTextRenderer::glyphInkWidth(const uint8_t* glyph) {
    // V1's dynamic kerning drops *every* all-blank column, not just trailing
    // ones. No supported glyph has an interior blank column, so counting
    // non-blank columns reproduces that behaviour exactly.
    uint8_t n = 0;
    for (uint8_t x = 0; x < MAX_WIDTH; x++) {
        if (glyph[x] != 0) n++;
    }
    return n;
}

// ── Public API ────────────────────────────────────────────────────────────────

uint8_t SmallTextRenderer::charWidth(char c) {
    return (uint8_t)(glyphInkWidth(getGlyph(c)) + KERNING);
}

uint16_t SmallTextRenderer::textWidth(const char* text) {
    if (!text) return 0;
    uint16_t w = 0;
    for (const char* p = text; *p; p++) {
        w = (uint16_t)(w + charWidth(*p));
    }
    return w;
}

uint32_t SmallTextRenderer::bufferSize(const char* text) {
    return (uint32_t)textWidth(text) * CHAR_HEIGHT * 3;
}

void SmallTextRenderer::renderText(const char* text, uint8_t* outPixels,
                                   uint16_t& outW, uint16_t& outH,
                                   const uint8_t* color) {
    outW = textWidth(text);
    outH = CHAR_HEIGHT;

    if (!outPixels || !text || outW == 0) return;

    memset(outPixels, 0, (size_t)outW * CHAR_HEIGHT * 3);

    uint16_t penX = 0;
    for (const char* p = text; *p; p++) {
        const uint8_t* glyph = getGlyph(*p);
        const uint8_t  ink   = glyphInkWidth(glyph);

        // Emit only the non-blank columns, then advance past the kern gap
        // (already zeroed by the memset above).
        uint8_t emitted = 0;
        for (uint8_t sx = 0; sx < MAX_WIDTH && emitted < ink; sx++) {
            const uint8_t col = glyph[sx];
            if (col == 0) continue;

            for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
                if ((col >> y) & 1) {
                    const uint32_t idx = ((uint32_t)y * outW + penX + emitted) * 3;
                    outPixels[idx]     = color[0];
                    outPixels[idx + 1] = color[1];
                    outPixels[idx + 2] = color[2];
                }
            }
            emitted++;
        }

        penX = (uint16_t)(penX + ink + KERNING);
    }
}
