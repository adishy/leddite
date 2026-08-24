#ifndef LIST_MENU_H
#define LIST_MENU_H

// ListMenu — reusable vertical submenu widget for the 16x16 panel.
//
// Pure C++/stdint only: no Arduino, no FastLED, no time source of its own.
// The caller supplies `nowMs`, which is what makes scroll animation and
// selection behaviour unit-testable and identical on native, WASM and ESP32.
//
// LAYOUT (16x16) — rows are NOT a fixed height
// -------------------------------------------
// The selected row is drawn in the 5x7 medium font and the others in the 3x4
// small font, so the row heights differ and the list reflows as the selection
// moves:
//
//   selected row    7px   (TextRenderer, 5x7)
//   other rows      4px   (SmallTextRenderer, 3x4)
//   y 15            position bar
//
// 7 + 4 + 4 = 15 exactly, which is why there are no gutters between rows. The
// size difference is what separates them; a 1px gap on top of that would not
// fit and is not needed to read them apart.
//
// Rendering rules:
//   - The selected row is drawn at full accent brightness in the larger font.
//     There is no filled background band: on a real WS2812B panel a lit
//     background against a lit glyph is much harder to read than a lit glyph
//     against unlit pixels, because the eye separates two similar lightnesses
//     far worse than it separates on from off. See docs/adr/0012.
//   - Unselected rows are dim, small, and clipped to their own band.
//   - Only the selected row scrolls, and only when its label exceeds 16px.
//     It dwells for DWELL_MS so the first characters are readable, then
//     scrolls at SCROLL_PPS and wraps with a GAP_PX gap.
//
// TextRenderer's stride is 6px, so at most 2 characters of the selected label
// are on screen at once and nearly every label scrolls. That is the accepted
// cost of the larger font: partial glyphs at the edges are fine, unreadable
// glyphs are not.

#include <stdint.h>

struct MenuItem {
    const char* label;
    uint8_t     r, g, b;   // accent colour
};

class ListMenu {
public:
    static const uint8_t  VISIBLE_ROWS = 3;
    static const uint8_t  SEL_ROW_H    = 7;   // 5x7 medium glyph
    static const uint8_t  ROW_H        = 4;   // 3x4 small glyph
    static const uint8_t  POS_BAR_Y    = 15;
    static const uint8_t  MAX_ITEMS    = 12;

    static const uint16_t DWELL_MS     = 1200;  // pause before a long label scrolls
    static const uint8_t  SCROLL_PPS   = 14;    // scroll speed, px/sec
    static const uint8_t  GAP_PX       = 6;     // gap between wrap repetitions
    static const uint8_t  DIM_SCALE    = 90;    // unselected-row text dim (0-255)

    // Floor for the selected row's colour, so a dark accent still reads as the
    // selection rather than disappearing. Applied to the text now that there is
    // no band behind it.
    static const uint8_t  SEL_MIN_PEAK = 140;

    // `items` must outlive the ListMenu — it is not copied.
    void begin(const MenuItem* items, uint8_t count, uint8_t selected, uint32_t nowMs);

    // Moves the selection, wrapping at both ends, and pans the viewport so the
    // selection stays visible. Resets the scroll animation.
    void turn(int delta, uint32_t nowMs);

    uint8_t selected() const { return sel; }
    uint8_t count()    const { return n; }
    uint8_t viewTop()  const { return top; }

    // Renders into a 16*16*3 buffer (see Draw.h).
    void render(uint8_t* buf, uint32_t nowMs) const;

    // Top pixel row of a visible row index (0..VISIBLE_ROWS-1), given which of
    // them is selected. Rows above a taller selected row are unaffected; rows
    // below it are pushed down. Exposed for tests.
    uint8_t rowTop(uint8_t row) const;

    // Height of a visible row index — SEL_ROW_H if it holds the selection.
    uint8_t rowHeight(uint8_t row) const;

    // The colour the selected row is drawn in: its accent, lifted if needed so
    // it clears SEL_MIN_PEAK. Exposed for tests.
    static void selColor(const MenuItem& item, uint8_t& r, uint8_t& g, uint8_t& b);

private:
    // Horizontal offset for a scrolling label at the given elapsed time.
    int16_t scrollOffset(uint16_t labelW, uint32_t nowMs) const;

    const MenuItem* list        = nullptr;
    uint8_t         n           = 0;
    uint8_t         sel         = 0;
    uint8_t         top         = 0;   // index shown in the first visible row
    uint32_t        scrollAnchorMs = 0;
};

#endif // LIST_MENU_H
