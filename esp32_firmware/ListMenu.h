#ifndef LIST_MENU_H
#define LIST_MENU_H

// ListMenu — reusable vertical submenu widget for the 16x16 panel.
//
// Pure C++/stdint only: no Arduino, no FastLED, no time source of its own.
// The caller supplies `nowMs`, which is what makes scroll animation and
// selection behaviour unit-testable and identical on native, WASM and ESP32.
//
// LAYOUT (16x16)
//   y  0- 4   row 0        3 visible rows of ROW_HEIGHT (4px glyph + 1px gap)
//   y  5- 9   row 1
//   y 10-14   row 2
//   y 15      position bar — shows where the selection sits in the full list
//
// Rendering rules:
//   - The selected row gets a full-width band in the item's accent colour with
//     the label knocked out in BLACK on top, which is what makes the selection
//     read as a solid highlight rather than as one more coloured row.
//   - Unselected rows are drawn dim on black and clipped at the row edge.
//   - Only the selected row scrolls, and only when its label exceeds 16px.
//     It dwells for DWELL_MS so the first characters are readable, then
//     scrolls at SCROLL_PPS and wraps with a GAP_PX gap.
//
// SmallTextRenderer advances 4px per letter, so labels of 4 characters or
// fewer never scroll.

#include <stdint.h>

struct MenuItem {
    const char* label;
    uint8_t     r, g, b;   // accent colour
};

class ListMenu {
public:
    static const uint8_t  VISIBLE_ROWS = 3;
    static const uint8_t  ROW_HEIGHT   = 5;   // 4px glyph + 1px gutter
    static const uint8_t  BAND_HEIGHT  = 4;   // highlight covers the glyph only,
                                              // leaving the gutter clear so the
                                              // band never abuts the next label
    static const uint8_t  POS_BAR_Y    = 15;
    static const uint8_t  MAX_ITEMS    = 12;

    static const uint16_t DWELL_MS     = 1200;  // pause before a long label scrolls
    static const uint8_t  SCROLL_PPS   = 12;    // scroll speed, px/sec
    static const uint8_t  GAP_PX       = 6;     // gap between wrap repetitions
    static const uint8_t  DIM_SCALE    = 90;    // unselected-row text dim (0-255)

    // The band carries the accent colour at full strength: black glyphs need the
    // background bright to read at all, so the old BAND_SCALE dimming is gone.
    // BAND_MIN_PEAK is a contrast floor — an item whose accent is too dark to
    // knock black text out of is scaled up until its brightest channel reaches
    // this, so the widget stays legible whatever palette a caller passes in.
    static const uint8_t  BAND_MIN_PEAK = 140;

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

    // The band colour actually used for an item — its accent, lifted if needed
    // so black text stays readable. Exposed for tests.
    static void bandColor(const MenuItem& item, uint8_t& r, uint8_t& g, uint8_t& b);

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
