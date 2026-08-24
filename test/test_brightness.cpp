#include "BrightnessModel.h"
#include "Draw.h"
#include "test_harness.h"

static uint8_t buf[Draw::SIZE];

void test_clamp_handles_garbage() {
    TEST("clampLevel survives corrupt/out-of-range input");
    // A corrupt NVS read must not index the level table out of bounds.
    ASSERT_EQ(BrightnessModel::clampLevel(0),      1,  "0 clamps up");
    ASSERT_EQ(BrightnessModel::clampLevel(-5),     1,  "negative clamps up");
    ASSERT_EQ(BrightnessModel::clampLevel(11),     10, "11 clamps down");
    ASSERT_EQ(BrightnessModel::clampLevel(255),    10, "255 clamps down");
    ASSERT_EQ(BrightnessModel::clampLevel(100000), 10, "huge clamps down");
    ASSERT_EQ(BrightnessModel::clampLevel(5),      5,  "in-range passes through");
    PASS();
}

void test_level_endpoints() {
    TEST("level endpoints");
    ASSERT_EQ(BrightnessModel::levelToFastLED(1),  20,  "level 1");
    ASSERT_EQ(BrightnessModel::levelToFastLED(10), 200, "level 10");
    ASSERT(BrightnessModel::levelToFastLED(10) < 255,
           "level 10 must not be full brightness — 256 white LEDs draw ~15 A");
    PASS();
}

void test_level_mapping_is_strictly_increasing() {
    TEST("brightness rises strictly with level");
    for (uint8_t l = 1; l < 10; l++)
        ASSERT(BrightnessModel::levelToFastLED((uint8_t)(l + 1)) >
               BrightnessModel::levelToFastLED(l),
               "levels must be strictly increasing");
    PASS();
}

void test_out_of_range_levels_are_safe() {
    TEST("levelToFastLED clamps rather than reading past the table");
    ASSERT_EQ(BrightnessModel::levelToFastLED(0),   20,  "level 0");
    ASSERT_EQ(BrightnessModel::levelToFastLED(200), 200, "level 200");
    PASS();
}

void test_worst_case_current() {
    TEST("worst-case current is monotonic and documented");
    uint32_t prev = 0;
    for (uint8_t l = 1; l <= 10; l++) {
        const uint32_t ma = BrightnessModel::worstCaseMilliamps(l);
        ASSERT(ma > prev, "current must rise with level");
        prev = ma;
    }
    // Levels 1-7 stay inside the supply budget even on an all-white frame.
    ASSERT(BrightnessModel::worstCaseMilliamps(7) <= BrightnessModel::MAX_MILLIAMPS,
           "level 7 should fit the budget unaided");
    // Levels 8-10 do not, which is precisely why the firmware must also call
    // FastLED.setMaxPowerInVoltsAndMilliamps(). The table alone is not enough.
    ASSERT(BrightnessModel::worstCaseMilliamps(10) > BrightnessModel::MAX_MILLIAMPS,
           "level 10 all-white should exceed budget, requiring FastLED's limiter");
    PASS();
}

void test_screen_bar_grows_with_level() {
    TEST("editor bar length grows with level");
    auto barWidth = [](uint8_t level) {
        BrightnessModel::renderScreen(buf, level);
        uint16_t n = 0;
        // Bar occupies y=9..11; count lit pixels on its middle row inside the track.
        for (uint8_t x = 1; x <= 14; x++) {
            const uint16_t i = (uint16_t)(10 * 16 + x) * 3;
            // Track is (14,14,16); the fill is much brighter.
            if (buf[i] > 100) n++;
        }
        return n;
    };

    uint16_t prev = 0;
    for (uint8_t l = 1; l <= 10; l++) {
        const uint16_t w = barWidth(l);
        ASSERT(w >= prev, "bar shrank as level increased");
        prev = w;
    }
    ASSERT(barWidth(1) >= 1,  "level 1 must still show a lit segment");
    ASSERT_EQ(barWidth(10), 14, "level 10 should fill the whole track");
    PASS();
}

void test_screen_is_never_blank() {
    TEST("editor screen always renders something");
    for (uint8_t l = 1; l <= 10; l++) {
        BrightnessModel::renderScreen(buf, l);
        bool lit = false;
        for (uint16_t i = 0; i < Draw::SIZE; i++) if (buf[i]) { lit = true; break; }
        ASSERT(lit, "brightness screen rendered blank");
    }
    PASS();
}

int main() {
    printf("=== BrightnessModel Unit Tests ===\n");
    test_clamp_handles_garbage();
    test_level_endpoints();
    test_level_mapping_is_strictly_increasing();
    test_out_of_range_levels_are_safe();
    test_worst_case_current();
    test_screen_bar_grows_with_level();
    test_screen_is_never_blank();
    SUMMARY("BrightnessModel");
}
