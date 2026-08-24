#include "ColorUtils.h"
#include "test_harness.h"

// These pin ColorUtils to FastLED's behaviour. If FastLED ever changes
// hsv2rgb_rainbow or sin8, these anchors are what catches the drift between the
// firmware's own FastLED calls and our port (see docs/adr/0001).

void test_sin8_anchors() {
    TEST("sin8 anchors match FastLED");
    ASSERT_EQ(ColorUtils::sin8(0),   128, "sin8(0)");
    ASSERT_EQ(ColorUtils::sin8(64),  255, "sin8(64) peak");
    ASSERT_EQ(ColorUtils::sin8(128), 128, "sin8(128)");
    ASSERT_EQ(ColorUtils::sin8(192), 1,   "sin8(192) trough");
    PASS();
}

void test_sin8_range_and_extrema() {
    TEST("sin8 stays in range, extrema at 64/192");
    int lo = 999, hi = -1, loAt = -1, hiAt = -1;
    for (int t = 0; t < 256; t++) {
        const int v = ColorUtils::sin8((uint8_t)t);
        ASSERT(v >= 0 && v <= 255, "sin8 out of byte range");
        if (v < lo) { lo = v; loAt = t; }
        if (v > hi) { hi = v; hiAt = t; }
    }
    ASSERT_EQ(hiAt, 64,  "sin8 max index");
    ASSERT_EQ(loAt, 192, "sin8 min index");
    PASS();
}

void test_sin8_monotonic_quarter() {
    TEST("sin8 rises monotonically over 0..64");
    for (int t = 0; t < 64; t++)
        ASSERT(ColorUtils::sin8((uint8_t)(t + 1)) >= ColorUtils::sin8((uint8_t)t),
               "sin8 not monotonic on the rising quarter");
    PASS();
}

void test_cos8_is_sin8_shifted() {
    TEST("cos8(t) == sin8(t + 64)");
    for (int t = 0; t < 256; t++)
        ASSERT(ColorUtils::cos8((uint8_t)t) == ColorUtils::sin8((uint8_t)(t + 64)),
               "cos8 is not sin8 phase-shifted");
    PASS();
}

void test_signed_wrappers() {
    TEST("sin8s/cos8s are the unsigned values minus 128");
    ASSERT_EQ(ColorUtils::sin8s(0),   0,   "sin8s(0)");
    ASSERT_EQ(ColorUtils::sin8s(64),  127, "sin8s(64)");
    ASSERT_EQ(ColorUtils::cos8s(0),   127, "cos8s(0)");
    ASSERT_EQ(ColorUtils::sin8s(192), -127, "sin8s(192)");
    PASS();
}

void test_scale8() {
    TEST("scale8 matches (i * scale) >> 8");
    ASSERT_EQ(ColorUtils::scale8(255, 255), 254, "scale8(255,255)");
    ASSERT_EQ(ColorUtils::scale8(255, 0),   0,   "scale8(255,0)");
    ASSERT_EQ(ColorUtils::scale8(0,   255), 0,   "scale8(0,255)");
    ASSERT_EQ(ColorUtils::scale8(128, 128), 64,  "scale8(128,128)");
    PASS();
}

void test_hsv_rainbow_anchors() {
    TEST("hsv2rgb_rainbow hue anchors match FastLED");
    struct { uint8_t h, r, g, b; const char* name; } A[] = {
        {  0, 255,   0,   0, "red"    },
        { 32, 171,  85,   0, "orange" },
        { 64, 171, 170,   0, "yellow" },
        { 96,   0, 255,   0, "green"  },
        {128,   0, 171,  85, "aqua"   },
        {160,   0,   0, 255, "blue"   },
        {192,  85,   0, 171, "purple" },
        {224, 170,   0,  85, "pink"   },
    };
    for (auto& a : A) {
        const RGB8 c = ColorUtils::hsv2rgb(a.h, 255, 255);
        ASSERT(c.r == a.r && c.g == a.g && c.b == a.b, a.name);
    }
    PASS();
}

void test_hsv_saturation_and_value_extremes() {
    TEST("sat=0 gives white, val=0 gives black");
    const RGB8 w = ColorUtils::hsv2rgb(0, 0, 255);
    ASSERT(w.r == 255 && w.g == 255 && w.b == 255, "sat=0 should be white");

    for (uint8_t h = 0; h < 250; h = (uint8_t)(h + 25)) {
        const RGB8 k = ColorUtils::hsv2rgb(h, 255, 0);
        ASSERT(k.r == 0 && k.g == 0 && k.b == 0, "val=0 should be black at any hue");
    }
    PASS();
}

void test_hsv_value_is_monotonic() {
    TEST("increasing value never darkens a colour");
    uint16_t prev = 0;
    for (uint16_t v = 0; v <= 255; v = (uint16_t)(v + 15)) {
        const RGB8 c = ColorUtils::hsv2rgb(0, 255, (uint8_t)v);
        const uint16_t sum = (uint16_t)(c.r + c.g + c.b);
        ASSERT(sum >= prev, "raising value darkened the output");
        prev = sum;
    }
    PASS();
}

int main() {
    printf("=== ColorUtils Unit Tests ===\n");
    test_sin8_anchors();
    test_sin8_range_and_extrema();
    test_sin8_monotonic_quarter();
    test_cos8_is_sin8_shifted();
    test_signed_wrappers();
    test_scale8();
    test_hsv_rainbow_anchors();
    test_hsv_saturation_and_value_extremes();
    test_hsv_value_is_monotonic();
    SUMMARY("ColorUtils");
}
