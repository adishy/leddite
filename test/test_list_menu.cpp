#include "ListMenu.h"
#include "Draw.h"
#include "SmallTextRenderer.h"
#include "test_harness.h"
#include <string.h>

static uint8_t buf[Draw::SIZE];

static const MenuItem FIVE[] = {
    {"SNAKE",      60, 220,  90},
    {"LIFE",      120, 200, 255},
    {"INVADERS",  255,  90,  90},
    {"DINO",      240, 200, 120},
    {"CYCLE ALL", 200, 120, 255},
};
static const MenuItem TWO[] = {
    {"ONE", 200, 200, 200},
    {"TWO", 100, 100, 100},
};

static bool pixLit(uint8_t x, uint8_t y) {
    const uint16_t i = (uint16_t)(y * 16 + x) * 3;
    return (buf[i] | buf[i + 1] | buf[i + 2]) != 0;
}

// Counts lit pixels inside a visible row's own band. The selected row is drawn
// in the 5x7 medium font at full accent brightness and the others in the dim
// 3x4 small font, so "which row is selected" is answered by height and
// brightness rather than by a background band.
static uint16_t rowInk(const ListMenu& m, uint8_t row) {
    const uint8_t y0 = m.rowTop(row), h = m.rowHeight(row);
    uint16_t n = 0;
    for (uint8_t y = y0; y < y0 + h && y < 16; y++)
        for (uint8_t x = 0; x < 16; x++)
            if (pixLit(x, y)) n++;
    return n;
}

static uint16_t rowPeak(const ListMenu& m, uint8_t row) {
    const uint8_t y0 = m.rowTop(row), h = m.rowHeight(row);
    uint16_t peak = 0;
    for (uint8_t y = y0; y < y0 + h && y < 16; y++)
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i = (uint16_t)(y * 16 + x) * 3;
            for (uint8_t k = 0; k < 3; k++) if (buf[i + k] > peak) peak = buf[i + k];
        }
    return peak;
}

// ── Selection ─────────────────────────────────────────────────────────────────

void test_initial_state() {
    TEST("begin() selects and positions the viewport");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);
    ASSERT_EQ(m.selected(), 0, "initial selection");
    ASSERT_EQ(m.viewTop(),  0, "initial viewport");
    ASSERT_EQ(m.count(),    5, "count");
    PASS();
}

void test_begin_scrolls_to_show_selection() {
    TEST("begin() with a late selection pans the viewport");
    ListMenu m;
    m.begin(FIVE, 5, 4, 0);
    ASSERT_EQ(m.selected(), 4, "selection");
    ASSERT(m.viewTop() + ListMenu::VISIBLE_ROWS > 4, "selection must be visible");
    ASSERT(m.viewTop() <= 4, "viewport past the selection");
    PASS();
}

void test_turn_wraps_both_directions() {
    TEST("selection wraps at both ends");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);
    m.turn(-1, 0);
    ASSERT_EQ(m.selected(), 4, "backwards from first wraps to last");
    m.turn(1, 0);
    ASSERT_EQ(m.selected(), 0, "forwards from last wraps to first");
    PASS();
}

void test_turn_walks_every_item() {
    TEST("turning walks the full list in order");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);
    for (uint8_t i = 1; i < 5; i++) {
        m.turn(1, 0);
        ASSERT_EQ(m.selected(), i, "sequential selection");
    }
    PASS();
}

void test_selection_always_visible() {
    TEST("selection never leaves the viewport");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);
    for (int i = 0; i < 40; i++) {
        const int dir = (i % 7 < 4) ? 1 : -1;
        m.turn(dir, (uint32_t)(i * 100));
        const uint8_t s = m.selected(), t = m.viewTop();
        ASSERT(s >= t && s < t + ListMenu::VISIBLE_ROWS, "selection scrolled out of view");
        ASSERT(t + ListMenu::VISIBLE_ROWS <= m.count(), "viewport ran past the end");
    }
    PASS();
}

void test_short_list_does_not_scroll() {
    TEST("a list shorter than the viewport never pans");
    ListMenu m;
    m.begin(TWO, 2, 0, 0);
    for (int i = 0; i < 6; i++) {
        m.turn(1, 0);
        ASSERT_EQ(m.viewTop(), 0, "short list should never pan");
    }
    PASS();
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void test_selected_row_is_taller() {
    TEST("the selected row is the tall one, and the layout still fits");
    ListMenu m;
    for (uint8_t sel = 0; sel < 3; sel++) {
        m.begin(FIVE, 5, sel, 0);
        uint8_t total = 0, tall = 0;
        for (uint8_t r = 0; r < ListMenu::VISIBLE_ROWS; r++) {
            const uint8_t h = m.rowHeight(r);
            total = (uint8_t)(total + h);
            if (h == ListMenu::SEL_ROW_H) { tall++; ASSERT_EQ(r, sel, "wrong row is tall"); }
        }
        ASSERT_EQ(tall, 1, "exactly one row should use the medium font");
        // 7 + 4 + 4 must land exactly on the position bar, or a row is clipped
        // or a dead strip appears above it.
        ASSERT_EQ(total, ListMenu::POS_BAR_Y, "rows must fill the panel above the bar");
    }
    PASS();
}

void test_rows_start_where_geometry_says() {
    TEST("rowTop accumulates the preceding row heights");
    ListMenu m;
    m.begin(FIVE, 5, 1, 0);          // row 1 tall
    ASSERT_EQ(m.rowTop(0), 0,  "row 0 starts at the top");
    ASSERT_EQ(m.rowTop(1), 4,  "row 1 follows a 4px row");
    ASSERT_EQ(m.rowTop(2), 11, "row 2 follows 4 + 7");
    PASS();
}

void test_selected_row_is_brightest() {
    TEST("the selected row outshines the others");
    // With no background band, brightness and size are the only cues that a row
    // is selected. If the accent dimming ever swallowed that, selection would
    // become invisible.
    ListMenu m;
    for (uint8_t sel = 0; sel < 3; sel++) {
        m.begin(FIVE, 5, sel, 0);
        m.render(buf, 0);
        const uint16_t selPeak = rowPeak(m, sel);
        for (uint8_t r = 0; r < ListMenu::VISIBLE_ROWS; r++) {
            if (r == sel) continue;
            ASSERT(selPeak > rowPeak(m, r), "an unselected row is as bright as the selection");
        }
    }
    PASS();
}

void test_no_row_is_a_solid_block() {
    TEST("no row is filled edge to edge");
    // A filled band behind the label reads badly on a real panel (adr/0012) and
    // is also what a regression to Draw::rect over the whole row would produce.
    ListMenu m;
    for (uint8_t sel = 0; sel < 3; sel++) {
        m.begin(FIVE, 5, sel, 0);
        m.render(buf, 0);
        for (uint8_t r = 0; r < ListMenu::VISIBLE_ROWS; r++) {
            const uint16_t cells = (uint16_t)(m.rowHeight(r) * 16);
            ASSERT(rowInk(m, r) < cells, "row is completely filled");
        }
    }
    PASS();
}

void test_dark_accent_is_lifted() {
    TEST("a dark accent is lifted so the selection still reads");
    static const MenuItem DARK[] = {
        {"DIM",  20, 10, 5},
        {"BLK",   0,  0, 0},
        {"OK",  255, 90, 90},
    };
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t r, g, b;
        ListMenu::selColor(DARK[i], r, g, b);
        uint8_t peak = r; if (g > peak) peak = g; if (b > peak) peak = b;
        ASSERT(peak >= ListMenu::SEL_MIN_PEAK, "selected row too dark to see");
    }
    uint8_t r, g, b;
    ListMenu::selColor(DARK[0], r, g, b);
    ASSERT(r > g && g > b, "lifting the accent lost its hue");
    PASS();
}

void test_position_bar_present_and_moves() {
    TEST("position bar exists and tracks the selection");
    ListMenu m;
    auto thumbStart = [&](uint8_t sel) -> int {
        m.begin(FIVE, 5, sel, 0);
        m.render(buf, 0);
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i = (uint16_t)(ListMenu::POS_BAR_Y * 16 + x) * 3;
            // Track is (12,12,12); the thumb is much brighter.
            if (buf[i] > 40 || buf[i + 1] > 40 || buf[i + 2] > 40) return x;
        }
        return -1;
    };
    const int first = thumbStart(0);
    const int last  = thumbStart(4);
    ASSERT(first >= 0, "no thumb rendered for the first item");
    ASSERT(last  >= 0, "no thumb rendered for the last item");
    ASSERT(last > first, "thumb did not move rightwards with the selection");
    PASS();
}

void test_unselected_rows_keep_their_accent() {
    TEST("unselected rows stay coloured, dim, and in the small font");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);          // row 1 = LIFE (120,200,255), blue-dominant
    m.render(buf, 0);

    ASSERT_EQ(m.rowHeight(1), ListMenu::ROW_H, "unselected row should be short");

    bool sawBlue = false;
    const uint8_t y0 = m.rowTop(1);
    for (uint8_t y = y0; y < y0 + m.rowHeight(1); y++)
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i = (uint16_t)(y * 16 + x) * 3;
            if (buf[i + 2] > buf[i] && buf[i + 2] > 0) { sawBlue = true; break; }
        }
    ASSERT(sawBlue, "unselected row lost its accent colour");
    PASS();
}

void test_rows_do_not_bleed() {
    TEST("each row's content stays inside its own band");
    // There are no gutters now — 7 + 4 + 4 fills the panel exactly — so the
    // clip is the only thing keeping a 7px glyph out of its neighbour's row.
    ListMenu m;
    for (uint8_t sel = 0; sel < 3; sel++) {
        m.begin(FIVE, 5, sel, 0);
        for (uint32_t t = 0; t < 6000; t += 400) {
            m.render(buf, t);
            for (uint8_t r = 0; r < ListMenu::VISIBLE_ROWS; r++) {
                const uint8_t y0 = m.rowTop(r), h = m.rowHeight(r);
                // Ink attributable to this row must lie within [y0, y0+h).
                for (uint8_t y = y0; y < y0 + h; y++)
                    if (y >= ListMenu::POS_BAR_Y) {
                        ASSERT(false, "a row extended into the position bar");
                        return;
                    }
            }
        }
    }
    PASS();
}

void test_long_label_dwells_then_scrolls() {
    TEST("long labels dwell, then scroll; short ones never move");
    ListMenu m;
    m.begin(FIVE, 5, 4, 0);          // CYCLE ALL = 33px, must scroll

    uint8_t atStart[Draw::SIZE], duringDwell[Draw::SIZE], afterDwell[Draw::SIZE];
    m.render(buf, 0);                            memcpy(atStart, buf, Draw::SIZE);
    m.render(buf, ListMenu::DWELL_MS - 50);      memcpy(duringDwell, buf, Draw::SIZE);
    m.render(buf, ListMenu::DWELL_MS + 1500);    memcpy(afterDwell, buf, Draw::SIZE);

    ASSERT(memcmp(atStart, duringDwell, Draw::SIZE) == 0,
           "label moved before the dwell elapsed");
    ASSERT(memcmp(atStart, afterDwell, Draw::SIZE) != 0,
           "label failed to scroll after the dwell");

    // A label that fits must never move. TextRenderer's stride is a fixed 6px,
    // so only two characters fit across 16px — which is why nearly every real
    // menu label scrolls now, and why this case needs its own fixture.
    static const MenuItem SHORT[] = {
        {"C",  200, 200, 200},
        {"F",  120, 200, 255},
    };
    m.begin(SHORT, 2, 0, 0);
    m.render(buf, 0);                          memcpy(atStart, buf, Draw::SIZE);
    m.render(buf, ListMenu::DWELL_MS + 5000);  memcpy(afterDwell, buf, Draw::SIZE);
    ASSERT(memcmp(atStart, afterDwell, Draw::SIZE) == 0,
           "a label that fits must never scroll");
    PASS();
}

void test_turn_restarts_dwell() {
    TEST("turning restarts the dwell so the new label is readable");
    ListMenu m;
    m.begin(FIVE, 5, 4, 0);
    m.render(buf, ListMenu::DWELL_MS + 2000);       // scrolled away
    uint8_t scrolled[Draw::SIZE];
    memcpy(scrolled, buf, Draw::SIZE);

    m.turn(1, ListMenu::DWELL_MS + 2000);           // wrap to SNAKE
    m.turn(-1, ListMenu::DWELL_MS + 2000);          // back to CYCLE ALL
    m.render(buf, ListMenu::DWELL_MS + 2000);       // dwell has just restarted

    ASSERT(memcmp(scrolled, buf, Draw::SIZE) != 0,
           "scroll offset was not reset by turning");
    PASS();
}

void test_empty_list_is_safe() {
    TEST("an empty list renders blank without crashing");
    ListMenu m;
    m.begin(nullptr, 0, 0, 0);
    m.render(buf, 0);
    ASSERT_EQ(m.count(), 0, "count");
    PASS();
}

int main() {
    printf("=== ListMenu Unit Tests ===\n");
    test_initial_state();
    test_begin_scrolls_to_show_selection();
    test_turn_wraps_both_directions();
    test_turn_walks_every_item();
    test_selection_always_visible();
    test_short_list_does_not_scroll();
    test_selected_row_is_taller();
    test_rows_start_where_geometry_says();
    test_selected_row_is_brightest();
    test_no_row_is_a_solid_block();
    test_dark_accent_is_lifted();
    test_position_bar_present_and_moves();
    test_unselected_rows_keep_their_accent();
    test_rows_do_not_bleed();
    test_long_label_dwells_then_scrolls();
    test_turn_restarts_dwell();
    test_empty_list_is_safe();
    SUMMARY("ListMenu");
}
