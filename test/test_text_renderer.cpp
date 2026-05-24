#include "TextRenderer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// ── Helper ────────────────────────────────────────────────────────────────────

static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
            testsFailed++; \
        } else { \
            testsPassed++; \
        } \
    } while(0)

#define TEST(name) \
    do { printf("  %-40s ", name); fflush(stdout); } while(0)

#define PASS() printf("OK\n")

// ── Tests ─────────────────────────────────────────────────────────────────────

void test_char_A_first_col() {
    TEST("test_char_A_first_col");
    // 'A' is at ASCII 0x41, offset = 0x41 - 0x20 = 33 in FONT_DATA
    ASSERT(TextRenderer::FONT_DATA[0x41 - 0x20][0] == 0x7E,
           "'A' first column should be 0x7E");
    PASS();
}

void test_font_matches_python_client() {
    TEST("test_font_matches_python_client");
    // Cross-check a few key glyphs against leddite_client.py FONT dict values
    // 'H': {0x7F, 0x08, 0x08, 0x08, 0x7F}
    const uint8_t* H = TextRenderer::FONT_DATA[0x48 - 0x20];
    ASSERT(H[0] == 0x7F && H[1] == 0x08 && H[4] == 0x7F,
           "'H' columns should be {0x7F, 0x08, 0x08, 0x08, 0x7F}");
    // 'I': {0x00, 0x41, 0x7F, 0x41, 0x00}
    const uint8_t* I = TextRenderer::FONT_DATA[0x49 - 0x20];
    ASSERT(I[0] == 0x00 && I[2] == 0x7F && I[4] == 0x00,
           "'I' columns should be {0x00, 0x41, 0x7F, 0x41, 0x00}");
    // '0': {0x3E, 0x51, 0x49, 0x45, 0x3E}
    const uint8_t* zero = TextRenderer::FONT_DATA[0x30 - 0x20];
    ASSERT(zero[0] == 0x3E && zero[4] == 0x3E,
           "'0' first and last columns should be 0x3E");
    PASS();
}

void test_width_calculation() {
    TEST("test_width_calculation");
    ASSERT(TextRenderer::textWidth("HI") == 12, "\"HI\" width should be 12");
    ASSERT(TextRenderer::textWidth("A") == 6,   "\"A\" width should be 6");
    ASSERT(TextRenderer::textWidth("CLK") == 18, "\"CLK\" width should be 18");
    ASSERT(TextRenderer::textWidth("") == 0,    "empty string width should be 0");
    PASS();
}

void test_height_is_always_7() {
    TEST("test_height_is_always_7");
    const uint8_t color[3] = {255, 255, 255};
    uint8_t buf[6 * 7 * 3] = {0};
    uint16_t w = 0, h = 0;
    TextRenderer::renderText("A", buf, w, h, color);
    ASSERT(h == 7, "height should always be 7");
    ASSERT(w == 6, "width of single char should be 6");
    PASS();
}

void test_empty_string_no_crash() {
    TEST("test_empty_string_no_crash");
    const uint8_t color[3] = {255, 0, 0};
    uint8_t buf[10] = {0};
    uint16_t w = 99, h = 99;
    TextRenderer::renderText("", buf, w, h, color);
    ASSERT(w == 0, "empty string should produce w=0");
    ASSERT(TextRenderer::textWidth("") == 0, "textWidth empty should be 0");
    ASSERT(TextRenderer::bufferSize("") == 0, "bufferSize empty should be 0");
    PASS();
}

void test_space_is_blank() {
    TEST("test_space_is_blank");
    const uint8_t color[3] = {255, 255, 255};
    uint8_t buf[6 * 7 * 3];
    memset(buf, 0xAB, sizeof(buf)); // poison buffer
    uint16_t w = 0, h = 0;
    TextRenderer::renderText(" ", buf, w, h, color);
    bool allZero = true;
    for (uint32_t i = 0; i < (uint32_t)w * h * 3; i++) {
        if (buf[i] != 0) { allZero = false; break; }
    }
    ASSERT(allZero, "space character should render as all-zero pixels");
    PASS();
}

void test_color_applied() {
    TEST("test_color_applied");
    const uint8_t color[3] = {255, 0, 128};
    uint8_t buf[6 * 7 * 3];
    memset(buf, 0, sizeof(buf));
    uint16_t w = 0, h = 0;
    TextRenderer::renderText("A", buf, w, h, color);
    // Find at least one lit pixel and check its color
    bool found = false;
    for (uint32_t i = 0; i < (uint32_t)w * h * 3; i += 3) {
        if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0) {
            ASSERT(buf[i]   == 255, "R channel should be 255");
            ASSERT(buf[i+1] == 0,   "G channel should be 0");
            ASSERT(buf[i+2] == 128, "B channel should be 128");
            found = true;
            break;
        }
    }
    ASSERT(found, "at least one lit pixel should exist in 'A'");
    PASS();
}

void test_colon_glyph() {
    TEST("test_colon_glyph");
    // ':' at 0x3A → offset 0x3A - 0x20 = 26
    const uint8_t* col = TextRenderer::FONT_DATA[0x3A - 0x20];
    // Pattern {0x00, 0x36, 0x36, 0x00, 0x00}: columns 1 and 2 are 0x36
    // 0x36 = 0011 0110: rows 1,2,4,5 set (two-dot colon effect)
    ASSERT(col[0] == 0x00, "':' col 0 should be 0x00");
    ASSERT(col[1] == 0x36, "':' col 1 should be 0x36");
    ASSERT(col[2] == 0x36, "':' col 2 should be 0x36");
    ASSERT(col[3] == 0x00, "':' col 3 should be 0x00");
    ASSERT(col[4] == 0x00, "':' col 4 should be 0x00");
    PASS();
}

void test_lowercase_treated_as_uppercase() {
    TEST("test_lowercase_treated_as_uppercase");
    const uint8_t color[3] = {255, 255, 255};
    uint8_t bufUpper[6 * 7 * 3], bufLower[6 * 7 * 3];
    uint16_t w1=0, h1=0, w2=0, h2=0;
    TextRenderer::renderText("A", bufUpper, w1, h1, color);
    TextRenderer::renderText("a", bufLower, w2, h2, color);
    ASSERT(w1 == w2 && h1 == h2, "upper and lowercase should have same dimensions");
    ASSERT(memcmp(bufUpper, bufLower, (uint32_t)w1*h1*3) == 0,
           "upper and lowercase 'a'/'A' should render identically");
    PASS();
}

void test_two_char_word_pixel_layout() {
    TEST("test_two_char_word_pixel_layout");
    // "HI" = two chars, 12px wide, 7px tall
    const uint8_t color[3] = {255, 255, 255};
    uint8_t buf[12 * 7 * 3];
    memset(buf, 0, sizeof(buf));
    uint16_t w=0, h=0;
    TextRenderer::renderText("HI", buf, w, h, color);
    ASSERT(w == 12, "\"HI\" should be 12px wide");
    ASSERT(h == 7,  "\"HI\" should be 7px tall");

    // 'H' first column (col 0) should have pixels at rows 0-6 (all bits in 0x7F set)
    // 0x7F = 0111 1111 → rows 0–6 all set
    bool firstColOk = true;
    for (uint8_t row = 0; row < 7; row++) {
        uint32_t idx = ((uint32_t)row * w + 0) * 3;
        if (buf[idx] == 0) { firstColOk = false; break; }
    }
    ASSERT(firstColOk, "'H' col 0 (0x7F) should have all 7 rows lit");
    PASS();
}

void test_buffer_size() {
    TEST("test_buffer_size");
    ASSERT(TextRenderer::bufferSize("A") == 6 * 7 * 3,    "single char buffer 6*7*3=126");
    ASSERT(TextRenderer::bufferSize("HI") == 12 * 7 * 3,  "\"HI\" buffer 12*7*3=252");
    ASSERT(TextRenderer::bufferSize("") == 0,              "empty string buffer 0");
    PASS();
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    printf("=== TextRenderer Unit Tests ===\n");

    test_char_A_first_col();
    test_font_matches_python_client();
    test_width_calculation();
    test_height_is_always_7();
    test_empty_string_no_crash();
    test_space_is_blank();
    test_color_applied();
    test_colon_glyph();
    test_lowercase_treated_as_uppercase();
    test_two_char_word_pixel_layout();
    test_buffer_size();

    printf("\n%d passed, %d failed\n", testsPassed, testsFailed);
    return testsFailed > 0 ? 1 : 0;
}
