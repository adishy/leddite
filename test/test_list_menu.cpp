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

// The selected row is the only one with a full-width band, so the far-right
// column of a row is a reliable probe: text never reaches x=15 on a short label.
static bool rowHasBand(uint8_t row) {
    return pixLit(15, (uint8_t)(row * ListMenu::ROW_HEIGHT));
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

void test_exactly_one_band() {
    TEST("exactly one row is highlighted");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);
    for (uint8_t s = 0; s < 5; s++) {
        m.begin(FIVE, 5, s, 0);
        m.render(buf, 0);
        uint8_t bands = 0;
        for (uint8_t r = 0; r < ListMenu::VISIBLE_ROWS; r++) if (rowHasBand(r)) bands++;
        ASSERT_EQ(bands, 1, "there must be exactly one highlighted row");
    }
    PASS();
}

void test_band_follows_selection() {
    TEST("the band sits on the selected row");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);
    m.render(buf, 0);
    ASSERT(rowHasBand(0), "row 0 should be banded when item 0 is selected");
    m.turn(1, 0);
    m.render(buf, 0);
    ASSERT(rowHasBand(1), "row 1 should be banded after one turn");
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

void test_selected_label_is_black_on_the_band() {
    TEST("the selected row knocks its label out in black");
    // Draw::blit treats black as transparent, so this only works via
    // Draw::stencilClipped. If it ever regresses to blit the glyph simply
    // disappears and the band becomes a solid bar — which is what this catches.
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);      // "SNAKE" — 20px, wider than the panel
    m.render(buf, 0);

    uint16_t black = 0, lit = 0;
    for (uint8_t x = 0; x < 16; x++)
        for (uint8_t y = 0; y < ListMenu::BAND_HEIGHT; y++)
            (pixLit(x, y) ? lit : black)++;

    ASSERT(black > 0, "no black glyph pixels — the label was not knocked out");
    ASSERT(lit   > 0, "no band pixels — the highlight is missing");
    PASS();
}

void test_band_is_bright_enough_for_black_text() {
    TEST("a dark accent is lifted so black text still reads");
    // Callers pick their own palettes; a dim accent would leave black-on-black.
    static const MenuItem DARK[] = {
        {"DIM",  20, 10, 5},
        {"BLK",   0,  0, 0},
        {"OK",  255, 90, 90},
    };
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t r, g, b;
        ListMenu::bandColor(DARK[i], r, g, b);
        uint8_t peak = r; if (g > peak) peak = g; if (b > peak) peak = b;
        ASSERT(peak >= ListMenu::BAND_MIN_PEAK, "band too dark for black text");
    }

    // A lifted accent keeps its hue — a red accent must not turn grey.
    uint8_t r, g, b;
    ListMenu::bandColor(DARK[0], r, g, b);
    ASSERT(r > g && g > b, "lifting the accent lost its hue");
    PASS();
}

void test_unselected_rows_keep_their_accent() {
    TEST("unselected rows stay coloured text on black");
    ListMenu m;
    m.begin(FIVE, 5, 0, 0);
    m.render(buf, 0);
    // Row 1 is "LIFE" (120,200,255) dimmed — blue-dominant, and its far-right
    // column is background because the label is 16px of mostly-blank advance.
    bool sawColour = false;
    for (uint8_t x = 0; x < 16; x++) {
        const uint16_t i = (uint16_t)((ListMenu::ROW_HEIGHT) * 16 + x) * 3;
        if (buf[i + 2] > buf[i] && buf[i + 2] > 0) { sawColour = true; break; }
    }
    ASSERT(sawColour, "unselected row lost its accent colour");
    PASS();
}

void test_rows_do_not_bleed() {
    TEST("row content is clipped to its own band");
    // A 4px glyph in a 5px row leaves row 4 (and 9, 14) as separator gaps.
    ListMenu m;
    m.begin(FIVE, 5, 1, 0);          // select row 1 so rows 0 and 2 are plain
    m.render(buf, 0);
    for (uint8_t x = 0; x < 16; x++) {
        ASSERT(!pixLit(x, 9),  "row 1's band must not fill the separator row");
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

    // "LIFE" is exactly 16px and must never scroll.
    m.begin(FIVE, 5, 1, 0);
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
    test_exactly_one_band();
    test_band_follows_selection();
    test_position_bar_present_and_moves();
    test_selected_label_is_black_on_the_band();
    test_band_is_bright_enough_for_black_text();
    test_unselected_rows_keep_their_accent();
    test_rows_do_not_bleed();
    test_long_label_dwells_then_scrolls();
    test_turn_restarts_dwell();
    test_empty_list_is_safe();
    SUMMARY("ListMenu");
}
