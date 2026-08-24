#include "ListMenu.h"
#include "Draw.h"
#include "SmallTextRenderer.h"
#include "TextRenderer.h"
#include <string.h>

// Scratch for one rendered label, in each font.
//   small  — 24 chars at the 4px worst-case advance = 96px
//   medium — 24 chars at TextRenderer's fixed 6px stride = 144px
static const uint16_t SMALL_MAX_W = 96;
static const uint16_t MED_MAX_W   = 144;
static uint8_t smallBuf[SMALL_MAX_W * SmallTextRenderer::CHAR_HEIGHT * 3];
static uint8_t medBuf[MED_MAX_W * TextRenderer::CHAR_HEIGHT * 3];

// ── Public API ────────────────────────────────────────────────────────────────

void ListMenu::begin(const MenuItem* items, uint8_t count, uint8_t selected,
                     uint32_t nowMs) {
    list = items;
    n    = (count > MAX_ITEMS) ? MAX_ITEMS : count;
    sel  = (n == 0) ? 0 : (uint8_t)(selected % n);

    // Position the viewport so the initial selection is visible.
    if (n <= VISIBLE_ROWS)              top = 0;
    else if (sel < VISIBLE_ROWS)        top = 0;
    else if (sel > n - VISIBLE_ROWS)    top = (uint8_t)(n - VISIBLE_ROWS);
    else                                top = (uint8_t)(sel - 1);

    scrollAnchorMs = nowMs;
}

void ListMenu::turn(int delta, uint32_t nowMs) {
    if (n == 0) return;

    sel = (uint8_t)(((int)sel + (int)n + (delta % (int)n)) % (int)n);

    if (n > VISIBLE_ROWS) {
        // Keep the selection inside the viewport. Wrapping in either direction
        // jumps the viewport to the corresponding end of the list.
        if (sel < top)                          top = sel;
        else if (sel >= top + VISIBLE_ROWS)     top = (uint8_t)(sel - VISIBLE_ROWS + 1);

        if (top > n - VISIBLE_ROWS)             top = (uint8_t)(n - VISIBLE_ROWS);
    } else {
        top = 0;
    }

    scrollAnchorMs = nowMs;   // restart dwell so the new label is readable
}

// ── Geometry ──────────────────────────────────────────────────────────────────
//
// Rows are not a fixed height: the selected one is taller because it uses the
// medium font. Everything below it shifts down accordingly.

uint8_t ListMenu::rowHeight(uint8_t row) const {
    if (!list || n == 0) return ROW_H;
    return ((uint8_t)(top + row) == sel) ? SEL_ROW_H : ROW_H;
}

uint8_t ListMenu::rowTop(uint8_t row) const {
    uint8_t y = 0;
    for (uint8_t r = 0; r < row; r++) y = (uint8_t)(y + rowHeight(r));
    return y;
}

// ── Colour ────────────────────────────────────────────────────────────────────

void ListMenu::selColor(const MenuItem& item, uint8_t& r, uint8_t& g, uint8_t& b) {
    uint8_t peak = item.r;
    if (item.g > peak) peak = item.g;
    if (item.b > peak) peak = item.b;

    if (peak == 0) {
        // A black accent that no scaling can rescue — fall back to neutral grey
        // rather than emitting an invisible row.
        r = g = b = SEL_MIN_PEAK;
        return;
    }
    if (peak >= SEL_MIN_PEAK) {
        r = item.r; g = item.g; b = item.b;
        return;
    }

    // Scale the whole triple so the hue is preserved and the brightest channel
    // lands exactly on the floor.
    const uint16_t k = (uint16_t)((SEL_MIN_PEAK * 256u) / peak);
    auto lift = [k](uint8_t c) -> uint8_t {
        const uint32_t v = ((uint32_t)c * k) >> 8;
        return (uint8_t)(v > 255 ? 255 : v);
    };
    r = lift(item.r);
    g = lift(item.g);
    b = lift(item.b);
}

// ── Scrolling ─────────────────────────────────────────────────────────────────

int16_t ListMenu::scrollOffset(uint16_t labelW, uint32_t nowMs) const {
    if (labelW <= Draw::W) return 0;        // fits: never scrolls

    const uint32_t elapsed = nowMs - scrollAnchorMs;
    if (elapsed <= DWELL_MS) return 0;      // dwell so the start is readable

    const uint32_t travel = (uint32_t)labelW + GAP_PX;
    const uint32_t moved  = ((elapsed - DWELL_MS) * SCROLL_PPS) / 1000u;

    return (int16_t)-(int16_t)(moved % travel);
}

// ── Render ────────────────────────────────────────────────────────────────────

void ListMenu::render(uint8_t* buf, uint32_t nowMs) const {
    Draw::clear(buf);
    if (!list || n == 0) return;

    const uint8_t rows = (n < VISIBLE_ROWS) ? n : VISIBLE_ROWS;

    for (uint8_t row = 0; row < rows; row++) {
        const uint8_t idx = (uint8_t)(top + row);
        if (idx >= n) break;

        const MenuItem& item  = list[idx];
        const int16_t   rowY  = (int16_t)rowTop(row);
        const int16_t   rowH  = (int16_t)rowHeight(row);
        const bool      isSel = (idx == sel);

        uint16_t w = 0, h = 0;

        if (isSel) {
            uint8_t cr, cg, cb;
            selColor(item, cr, cg, cb);

            if (TextRenderer::textWidth(item.label) > MED_MAX_W) continue;

            // Bright accent on black — no filled band. See docs/adr/0012.
            const uint8_t tc[3] = { cr, cg, cb };
            TextRenderer::renderText(item.label, medBuf, w, h, tc);
            if (w == 0) continue;

            const int16_t xOff = scrollOffset(w, nowMs);

            Draw::blitClipped(buf, medBuf, w, h, xOff, rowY, rowY, rowH);
            // Second copy so a wrapping scroll has no blank gap at the seam.
            if (w > Draw::W)
                Draw::blitClipped(buf, medBuf, w, h, (int16_t)(xOff + w + GAP_PX),
                                  rowY, rowY, rowH);
        } else {
            uint8_t tc[3] = {
                Draw::dim(item.r, DIM_SCALE),
                Draw::dim(item.g, DIM_SCALE),
                Draw::dim(item.b, DIM_SCALE),
            };
            // A fully-dimmed accent could round to black, which blit() treats as
            // transparent and would make the row vanish. Keep a visible floor.
            if ((tc[0] | tc[1] | tc[2]) == 0) tc[0] = tc[1] = tc[2] = 24;

            if (SmallTextRenderer::textWidth(item.label) > SMALL_MAX_W) continue;
            SmallTextRenderer::renderText(item.label, smallBuf, w, h, tc);
            if (w == 0) continue;

            Draw::blitClipped(buf, smallBuf, w, h, 0, rowY, rowY, rowH);
        }
    }

    // ── Position bar ──────────────────────────────────────────────────────────
    // A thumb whose width is proportional to how much of the list is visible,
    // slid to reflect the selection. Always at least 2px so it stays legible.
    const MenuItem& cur = list[sel];

    uint8_t thumbW = (uint8_t)((uint16_t)Draw::W * VISIBLE_ROWS / (n ? n : 1));
    if (thumbW < 2)        thumbW = 2;
    if (thumbW > Draw::W)  thumbW = Draw::W;

    const uint8_t travel = (uint8_t)(Draw::W - thumbW);
    const uint8_t thumbX = (n > 1) ? (uint8_t)((uint16_t)travel * sel / (n - 1)) : 0;

    // Dim track, then the bright thumb over it.
    Draw::rect(buf, 0, POS_BAR_Y, Draw::W, 1, 12, 12, 12);
    Draw::rect(buf, thumbX, POS_BAR_Y, thumbW, 1,
               Draw::dim(cur.r, 200), Draw::dim(cur.g, 200), Draw::dim(cur.b, 200));
}
