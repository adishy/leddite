#include "SmallTextRenderer.h"
#include "test_harness.h"
#include <string.h>

// The width table below is the whole point of this font: SmallTextRenderer
// advances 4px per letter, so exactly 4 characters fit a 16px row. Menu layout,
// the temperature row and the place codes all depend on these numbers.

void test_char_height_is_four() {
    TEST("glyphs are 4px tall");
    ASSERT_EQ(SmallTextRenderer::CHAR_HEIGHT, 4, "CHAR_HEIGHT");
    uint8_t buf[64 * 4 * 3];
    uint16_t w = 0, h = 0;
    const uint8_t c[3] = {255, 255, 255};
    SmallTextRenderer::renderText("ABC", buf, w, h, c);
    ASSERT_EQ(h, 4, "renderText outH");
    PASS();
}

void test_advance_widths() {
    TEST("per-character advance widths");
    ASSERT_EQ(SmallTextRenderer::charWidth('A'), 4, "'A'");
    ASSERT_EQ(SmallTextRenderer::charWidth('Z'), 4, "'Z'");
    ASSERT_EQ(SmallTextRenderer::charWidth('0'), 4, "'0'");
    ASSERT_EQ(SmallTextRenderer::charWidth('-'), 4, "'-'");
    ASSERT_EQ(SmallTextRenderer::charWidth('/'), 4, "'/'");
    ASSERT_EQ(SmallTextRenderer::charWidth('1'), 3, "'1' is narrow");
    ASSERT_EQ(SmallTextRenderer::charWidth(':'), 3, "':'");
    ASSERT_EQ(SmallTextRenderer::charWidth('.'), 3, "'.'");
    ASSERT_EQ(SmallTextRenderer::charWidth('*'), 3, "'*' degree mark");
    ASSERT_EQ(SmallTextRenderer::charWidth('!'), 2, "'!'");
    ASSERT_EQ(SmallTextRenderer::charWidth(' '), 1, "' '");
    PASS();
}

void test_label_widths() {
    TEST("menu label widths (drives scroll behaviour)");
    ASSERT_EQ(SmallTextRenderer::textWidth("LIFE"),       16, "LIFE");
    ASSERT_EQ(SmallTextRenderer::textWidth("DINO"),       16, "DINO");
    ASSERT_EQ(SmallTextRenderer::textWidth("ALL"),        12, "ALL");
    ASSERT_EQ(SmallTextRenderer::textWidth("NYC"),        12, "NYC");
    ASSERT_EQ(SmallTextRenderer::textWidth("CAMB"),       16, "CAMB");
    ASSERT_EQ(SmallTextRenderer::textWidth("SNAKE"),      20, "SNAKE scrolls");
    ASSERT_EQ(SmallTextRenderer::textWidth("INVADERS"),   32, "INVADERS scrolls");
    ASSERT_EQ(SmallTextRenderer::textWidth("CYCLE ALL"),  33, "CYCLE ALL scrolls");
    ASSERT_EQ(SmallTextRenderer::textWidth("BRIGHTNESS"), 40, "BRIGHTNESS scrolls");
    PASS();
}

void test_temperature_strings_fit_one_row() {
    TEST("every realistic temperature fits 16px");
    // This is why the degree symbol was dropped: "-12*C" is 18px and would
    // scroll. Without it even -40C/-60C land at exactly 16px.
    const char* T[] = { "0C", "12C", "-5C", "-12C", "-40C", "-60C",
                        "32F", "100F", "130F", "-40F", "--C", "--F" };
    for (auto t : T) {
        const uint16_t w = SmallTextRenderer::textWidth(t);
        ASSERT(w <= 16, t);
    }
    ASSERT(SmallTextRenderer::textWidth("-12*C") > 16,
           "sanity: with a degree mark it would NOT fit");
    PASS();
}

void test_place_codes_never_scroll() {
    TEST("all place codes fit without scrolling");
    const char* P[] = { "NYC", "CAMB", "SFO", "SLL", "MCT", "AMH", "TVM" };
    for (auto p : P) ASSERT(SmallTextRenderer::textWidth(p) <= 16, p);
    PASS();
}

void test_lowercase_folds_to_uppercase() {
    TEST("lowercase folds to uppercase");
    ASSERT_EQ(SmallTextRenderer::textWidth("snake"),
              SmallTextRenderer::textWidth("SNAKE"), "width parity");

    uint8_t a[32 * 4 * 3], b[32 * 4 * 3];
    uint16_t aw = 0, ah = 0, bw = 0, bh = 0;
    const uint8_t c[3] = {9, 9, 9};
    SmallTextRenderer::renderText("snake", a, aw, ah, c);
    SmallTextRenderer::renderText("SNAKE", b, bw, bh, c);
    ASSERT(memcmp(a, b, (size_t)aw * ah * 3) == 0, "pixels differ between cases");
    PASS();
}

void test_unsupported_chars_render_as_space() {
    TEST("unsupported characters become spaces");
    ASSERT_EQ(SmallTextRenderer::charWidth('#'), 1, "'#' should be blank");
    ASSERT_EQ(SmallTextRenderer::charWidth('~'), 1, "out-of-range char");
    PASS();
}

void test_new_glyphs_are_not_blank() {
    TEST("'-' and '*' are real glyphs, not blanks");
    // V1's FontSmall had neither; sub-zero temperatures need the minus.
    const uint8_t* minus = SmallTextRenderer::FONT_DATA['-' - 0x20];
    const uint8_t* deg   = SmallTextRenderer::FONT_DATA['*' - 0x20];
    ASSERT(minus[0] || minus[1] || minus[2], "'-' glyph is blank");
    ASSERT(deg[0] || deg[1], "'*' glyph is blank");
    PASS();
}

void test_pixel_layout_of_L() {
    TEST("'L' pixel layout is column-major, LSB = top");
    // 'L' is {0x0F, 0x08, 0x08, 0x00}: full left column, then two bottom-row pixels.
    uint8_t buf[8 * 4 * 3];
    uint16_t w = 0, h = 0;
    const uint8_t c[3] = {200, 100, 50};
    SmallTextRenderer::renderText("L", buf, w, h, c);
    ASSERT_EQ(w, 4, "'L' advance");

    auto lit = [&](int x, int y) {
        return buf[((size_t)y * w + x) * 3] != 0;
    };
    ASSERT(lit(0, 0) && lit(0, 1) && lit(0, 2) && lit(0, 3), "left stem missing");
    ASSERT(lit(1, 3) && lit(2, 3), "bottom bar missing");
    ASSERT(!lit(1, 0) && !lit(2, 0), "top row should be clear except the stem");
    ASSERT(!lit(3, 0) && !lit(3, 3), "kern column should be blank");
    PASS();
}

void test_colour_is_applied() {
    TEST("requested colour lands in the buffer");
    uint8_t buf[8 * 4 * 3];
    uint16_t w = 0, h = 0;
    const uint8_t c[3] = {12, 34, 56};
    SmallTextRenderer::renderText("I", buf, w, h, c);
    bool found = false;
    for (size_t i = 0; i + 2 < (size_t)w * h * 3; i += 3)
        if (buf[i] == 12 && buf[i + 1] == 34 && buf[i + 2] == 56) { found = true; break; }
    ASSERT(found, "colour not present in rendered pixels");
    PASS();
}

void test_empty_and_null_are_safe() {
    TEST("empty and null strings do not crash");
    uint8_t buf[16];
    uint16_t w = 99, h = 99;
    const uint8_t c[3] = {1, 2, 3};
    SmallTextRenderer::renderText("", buf, w, h, c);
    ASSERT_EQ(w, 0, "empty width");
    ASSERT_EQ(SmallTextRenderer::textWidth(nullptr), 0, "null width");
    PASS();
}

void test_buffer_size_matches_render() {
    TEST("bufferSize == textWidth * height * 3");
    ASSERT_EQ(SmallTextRenderer::bufferSize("SNAKE"), 20u * 4u * 3u, "SNAKE");
    ASSERT_EQ(SmallTextRenderer::bufferSize("NYC"),   12u * 4u * 3u, "NYC");
    PASS();
}

int main() {
    printf("=== SmallTextRenderer Unit Tests ===\n");
    test_char_height_is_four();
    test_advance_widths();
    test_label_widths();
    test_temperature_strings_fit_one_row();
    test_place_codes_never_scroll();
    test_lowercase_folds_to_uppercase();
    test_unsupported_chars_render_as_space();
    test_new_glyphs_are_not_blank();
    test_pixel_layout_of_L();
    test_colour_is_applied();
    test_empty_and_null_are_safe();
    test_buffer_size_matches_render();
    SUMMARY("SmallTextRenderer");
}
