#include "ListMenu.h"
#include "Draw.h"
#include "SmallTextRenderer.h"
#include <string.h>

// Scratch for one rendered label. MAX label is bounded by the buffer below:
// 24 characters at the 4px worst-case advance = 96px.
static const uint16_t LABEL_MAX_W = 96;
static uint8_t        labelBuf[LABEL_MAX_W * SmallTextRenderer::CHAR_HEIGHT * 3];

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
        const uint8_t   idx  = (uint8_t)(top + row);
        if (idx >= n) break;

        const MenuItem& item = list[idx];
        const int16_t   rowY = (int16_t)(row * ROW_HEIGHT);
        const bool      isSel = (idx == sel);

        uint8_t textColor[3];

        if (isSel) {
            // Accent band across the full row width, text bright on top.
            Draw::rect(buf, 0, rowY, Draw::W, BAND_HEIGHT,
                       Draw::dim(item.r, BAND_SCALE),
                       Draw::dim(item.g, BAND_SCALE),
                       Draw::dim(item.b, BAND_SCALE));
            textColor[0] = 255; textColor[1] = 255; textColor[2] = 255;
        } else {
            textColor[0] = Draw::dim(item.r, DIM_SCALE);
            textColor[1] = Draw::dim(item.g, DIM_SCALE);
            textColor[2] = Draw::dim(item.b, DIM_SCALE);

            // A fully-dimmed accent could round to black, which blit() treats as
            // transparent and would make the row vanish. Keep a visible floor.
            if ((textColor[0] | textColor[1] | textColor[2]) == 0) {
                textColor[0] = textColor[1] = textColor[2] = 24;
            }
        }

        uint16_t w = 0, h = 0;
        if (SmallTextRenderer::textWidth(item.label) > LABEL_MAX_W) continue;
        SmallTextRenderer::renderText(item.label, labelBuf, w, h, textColor);
        if (w == 0) continue;

        const int16_t xOff = isSel ? scrollOffset(w, nowMs) : 0;

        // Glyphs are 4px in a 5px row; the trailing pixel row is the gap.
        Draw::blitClipped(buf, labelBuf, w, h, xOff, rowY, rowY, BAND_HEIGHT);

        // Second copy so a wrapping scroll has no blank gap at the seam.
        if (isSel && w > Draw::W) {
            Draw::blitClipped(buf, labelBuf, w, h,
                              (int16_t)(xOff + w + GAP_PX), rowY, rowY, BAND_HEIGHT);
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
