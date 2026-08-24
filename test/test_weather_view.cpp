#include "WeatherView.h"
#include "SmallTextRenderer.h"
#include "TextRenderer.h"
#include "Places.h"
#include "Draw.h"
#include "test_harness.h"
#include <string.h>

static uint8_t buf[Draw::SIZE];

static bool anyLit(const uint8_t* b) {
    for (uint16_t i = 0; i < Draw::SIZE; i++) if (b[i]) return true;
    return false;
}

static bool rowLit(const uint8_t* b, uint8_t y) {
    for (uint8_t x = 0; x < 16; x++) {
        const uint16_t i = (uint16_t)(y * 16 + x) * 3;
        if (b[i] | b[i + 1] | b[i + 2]) return true;
    }
    return false;
}

// Every WMO code Open-Meteo documents, so the description and colour tables are
// checked against the real domain rather than a sample.
static const uint8_t ALL_CODES[] = {
    0, 1, 2, 3, 45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66, 67,
    71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99,
};

// ── Descriptions ──────────────────────────────────────────────────────────────

void test_every_code_has_a_description() {
    TEST("every documented WMO code has a non-empty description");
    for (uint8_t c : ALL_CODES) {
        for (uint8_t day = 0; day < 2; day++) {
            const char* s = WeatherView::describe(c, day != 0);
            ASSERT(s != nullptr && s[0] != '\0', "empty description");
            ASSERT(strcmp(s, "UNKNOWN") != 0, "documented code fell through to UNKNOWN");
        }
    }
    PASS();
}

void test_undocumented_codes_are_unknown() {
    TEST("codes outside the WMO set read as UNKNOWN");
    const uint8_t BAD[] = { 4, 10, 30, 44, 49, 50, 58, 60, 68, 70, 78, 79,
                            83, 84, 87, 94, 97, 100, 200, 255 };
    for (uint8_t c : BAD)
        ASSERT(strcmp(WeatherView::describe(c, true), "UNKNOWN") == 0,
               "should be UNKNOWN");
    PASS();
}

void test_descriptions_fit_the_scratch_buffer() {
    TEST("no description exceeds DESC_MAX_CHARS");
    // The scratch buffer is sized from this constant, so an over-long string
    // added later would be silently dropped by render()'s guard. Catch it here.
    for (uint8_t c : ALL_CODES) {
        for (uint8_t day = 0; day < 2; day++) {
            const char* s = WeatherView::describe(c, day != 0);
            if (strlen(s) > WeatherView::DESC_MAX_CHARS) {
                ASSERT(false, "description longer than DESC_MAX_CHARS");
                return;
            }
        }
    }
    ASSERT(strlen("NO DATA") <= WeatherView::DESC_MAX_CHARS, "fallback string fits");
    PASS();
}

void test_longest_description_scrolls_fully_in_time() {
    TEST("the longest description completes a pass before the view rotates");
    // TimeMode gives this view VIEW_BUDGET_MS, of which PLACE_FLASH_MS is spent
    // on the place code and DESC_DWELL_MS on the opening dwell. If a full scroll
    // cycle does not fit in the remainder, the end of the longest condition is
    // never seen — which is a silent failure, not a visible one.
    uint16_t widest = 0;
    for (uint8_t c : ALL_CODES)
        for (uint8_t day = 0; day < 2; day++) {
            const uint16_t w = TextRenderer::textWidth(WeatherView::describe(c, day != 0));
            if (w > widest) widest = w;
        }

    const uint32_t travel   = (uint32_t)widest + WeatherView::DESC_GAP_PX;
    const uint32_t scrollMs = (travel * 1000u) / WeatherView::DESC_PPS;
    const uint32_t needed   = scrollMs + WeatherView::DESC_DWELL_MS;
    const uint32_t budget   = (uint32_t)WeatherView::VIEW_BUDGET_MS
                            - WeatherView::PLACE_FLASH_MS;

    ASSERT(needed <= budget, "longest description cannot finish a pass in time");
    PASS();
}

void test_descriptions_are_renderable() {
    TEST("descriptions use only glyphs the 5x7 font actually has");
    // Unsupported characters render as blanks, which would silently gap a word.
    for (uint8_t c : ALL_CODES) {
        for (uint8_t day = 0; day < 2; day++) {
            const char* s = WeatherView::describe(c, day != 0);
            for (const char* p = s; *p; p++) {
                const bool ok = (*p >= 'A' && *p <= 'Z') ||
                                (*p >= '0' && *p <= '9') ||
                                *p == ' ' || *p == '-' || *p == '.' || *p == ':';
                if (!ok) { ASSERT(false, "description holds an unrenderable character"); return; }
            }
        }
    }
    PASS();
}

void test_clear_sky_reads_differently_at_night() {
    TEST("clear sky is worded for day and night");
    ASSERT(strcmp(WeatherView::describe(0, true), WeatherView::describe(0, false)) != 0,
           "clear day and clear night should not read the same");
    PASS();
}

void test_intensity_is_distinguished() {
    TEST("slight / moderate / heavy read differently");
    ASSERT(strcmp(WeatherView::describe(61, true), WeatherView::describe(63, true)) != 0,
           "light vs moderate rain");
    ASSERT(strcmp(WeatherView::describe(63, true), WeatherView::describe(65, true)) != 0,
           "moderate vs heavy rain");
    ASSERT(strcmp(WeatherView::describe(71, true), WeatherView::describe(75, true)) != 0,
           "light vs heavy snow");
    PASS();
}

// ── Colours ───────────────────────────────────────────────────────────────────

void test_condition_colors_are_visible() {
    TEST("no condition maps to a black (invisible) colour");
    // Draw::blit treats pure black as transparent, so a black accent would make
    // the description vanish entirely rather than merely look wrong.
    for (uint8_t c = 0; c < 100; c++) {
        for (uint8_t day = 0; day < 2; day++) {
            uint8_t r, g, b;
            WeatherView::conditionColor(c, day != 0, r, g, b);
            if ((r | g | b) == 0) { ASSERT(false, "condition colour is black"); return; }
        }
    }
    PASS();
}

void test_condition_colors_separate_states() {
    TEST("rain, snow, sun and fog are visibly different colours");
    auto col = [](uint8_t c, bool day) {
        uint8_t r, g, b;
        WeatherView::conditionColor(c, day, r, g, b);
        return (uint32_t)r << 16 | (uint32_t)g << 8 | b;
    };
    ASSERT(col(63, true) != col(73, true),  "rain vs snow");
    ASSERT(col(0,  true) != col(0,  false), "clear day vs clear night");
    ASSERT(col(0,  true) != col(45, true),  "sun vs fog");
    ASSERT(col(63, true) != col(66, true),  "rain vs freezing rain");
    PASS();
}

void test_freezing_flag() {
    TEST("freezing drizzle and rain are flagged");
    ASSERT(WeatherView::isFreezing(56) && WeatherView::isFreezing(57), "freezing drizzle");
    ASSERT(WeatherView::isFreezing(66) && WeatherView::isFreezing(67), "freezing rain");
    ASSERT(!WeatherView::isFreezing(61) && !WeatherView::isFreezing(71), "not freezing");
    PASS();
}

// ── Temperature ───────────────────────────────────────────────────────────────

void test_celsius_rounding() {
    TEST("Celsius rounds half away from zero");
    ASSERT_EQ(WeatherView::displayTemp(0,    TempUnit::CELSIUS),  0,   "0.0");
    ASSERT_EQ(WeatherView::displayTemp(124,  TempUnit::CELSIUS),  12,  "12.4 down");
    ASSERT_EQ(WeatherView::displayTemp(125,  TempUnit::CELSIUS),  13,  "12.5 up");
    ASSERT_EQ(WeatherView::displayTemp(-124, TempUnit::CELSIUS), -12,  "-12.4 up");
    ASSERT_EQ(WeatherView::displayTemp(-125, TempUnit::CELSIUS), -13,  "-12.5 down");
    PASS();
}

void test_fahrenheit_conversion() {
    TEST("Fahrenheit conversion incl. the -40 crossover");
    ASSERT_EQ(WeatherView::displayTemp(0,    TempUnit::FAHRENHEIT),  32,  "0C = 32F");
    ASSERT_EQ(WeatherView::displayTemp(1000, TempUnit::FAHRENHEIT),  212, "100C = 212F");
    ASSERT_EQ(WeatherView::displayTemp(-400, TempUnit::FAHRENHEIT), -40,  "-40C = -40F");
    ASSERT_EQ(WeatherView::displayTemp(370,  TempUnit::FAHRENHEIT),  99,  "37C = 99F");
    PASS();
}

void test_temperature_always_carries_a_degree_mark() {
    TEST("every formatted temperature keeps its degree mark");
    // The unit letter is the part that may be shed on overflow; the degree
    // never is. Sweep the whole plausible range in both units.
    for (int16_t c10 = -700; c10 <= 600; c10 = (int16_t)(c10 + 1)) {
        for (uint8_t u = 0; u < 2; u++) {
            char s[10];
            WeatherView::formatTemp(c10, (TempUnit)u, s, sizeof(s));
            if (strchr(s, SmallTextRenderer::DEGREE_CHAR) == nullptr) {
                ASSERT(false, "formatted temperature lost its degree mark");
                return;
            }
        }
    }
    PASS();
}

void test_formatted_temperatures_always_fit() {
    TEST("formatted temperature always fits a 16px row");
    // The layout has no scroll fallback on the temperature row, so this is a
    // hard constraint, not a preference.
    for (int16_t c10 = -700; c10 <= 600; c10 = (int16_t)(c10 + 1)) {
        for (uint8_t u = 0; u < 2; u++) {
            char s[10];
            WeatherView::formatTemp(c10, (TempUnit)u, s, sizeof(s));
            if (SmallTextRenderer::textWidth(s) > Draw::W) {
                ASSERT(false, "temperature string overflows the row");
                return;
            }
        }
    }
    PASS();
}

void test_format_strings() {
    TEST("formatTemp output strings");
    char s[10];
    WeatherView::formatTemp(123,  TempUnit::CELSIUS,    s, sizeof(s));
    ASSERT(strcmp(s, "12*C") == 0, "12.3C -> 12*C");
    WeatherView::formatTemp(-52,  TempUnit::CELSIUS,    s, sizeof(s));
    ASSERT(strcmp(s, "-5*C") == 0, "-5.2C -> -5*C");

    // 212*F is 18px, so the unit letter goes and the degree stays.
    WeatherView::formatTemp(1000, TempUnit::FAHRENHEIT, s, sizeof(s));
    ASSERT(strcmp(s, "212*") == 0, "100C -> 212* (unit shed to fit)");
    WeatherView::formatTemp(-120, TempUnit::CELSIUS,    s, sizeof(s));
    ASSERT(strcmp(s, "-12*") == 0, "-12C -> -12* (unit shed to fit)");
    PASS();
}

// ── Rendering ─────────────────────────────────────────────────────────────────

static WeatherData reading(int16_t c10, uint8_t code, bool day) {
    WeatherData d;
    d.valid = true; d.tempC10 = c10; d.wmoCode = code; d.isDay = day;
    return d;
}

static const uint32_t STEADY = WeatherView::PLACE_FLASH_MS + 100;

void test_every_code_renders_something() {
    TEST("every condition renders visible pixels");
    for (uint8_t c : ALL_CODES) {
        for (uint8_t day = 0; day < 2; day++) {
            WeatherView::render(buf, reading(180, c, day != 0),
                                TempUnit::CELSIUS, "CAMB", STEADY);
            if (!anyLit(buf)) { ASSERT(false, "condition rendered blank"); return; }
        }
    }
    PASS();
}

void test_layout_bands_are_respected() {
    TEST("temperature and description stay in their own rows");
    // The temperature occupies rows 0-3, the rule sits on row 5 and the
    // description runs 8-14. Rows 4, 6, 7 and 15 are structural gaps: content
    // bleeding into them is what made the old icon layout unreadable.
    for (uint8_t c : ALL_CODES) {
        for (uint32_t t = STEADY; t < STEADY + 6000; t += 700) {
            WeatherView::render(buf, reading(-125, c, true),
                                TempUnit::FAHRENHEIT, "NYC", t);
            const uint8_t GAPS[] = { 4, 5, 6, 7, 15 };
            for (uint8_t y : GAPS) {
                if (rowLit(buf, y)) { ASSERT(false, "content bled into a gap row"); return; }
            }
        }
    }
    PASS();
}

void test_description_carries_the_condition_hue() {
    TEST("the description carries the condition's hue");
    WeatherView::render(buf, reading(120, 63, true), TempUnit::CELSIUS, "NYC", STEADY);
    uint32_t r = 0, g = 0, b = 0;
    for (uint8_t y = WeatherView::DESC_Y; y < WeatherView::DESC_Y + 7; y++)
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i = (uint16_t)(y * 16 + x) * 3;
            r += buf[i]; g += buf[i + 1]; b += buf[i + 2];
        }
    ASSERT(b > r, "rain should read blue-dominant");

    WeatherView::render(buf, reading(120, 0, true), TempUnit::CELSIUS, "NYC", STEADY);
    r = g = b = 0;
    for (uint8_t y = WeatherView::DESC_Y; y < WeatherView::DESC_Y + 7; y++)
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i = (uint16_t)(y * 16 + x) * 3;
            r += buf[i]; g += buf[i + 1]; b += buf[i + 2];
        }
    ASSERT(r > b, "clear day should read warm-dominant");
    PASS();
}

void test_description_dwells_then_scrolls() {
    TEST("the description dwells, then scrolls");
    const WeatherData d = reading(180, 2, true);   // PARTLY CLOUDY, 78px

    uint8_t atStart[Draw::SIZE], duringDwell[Draw::SIZE], afterDwell[Draw::SIZE];
    WeatherView::render(buf, d, TempUnit::CELSIUS, "NYC", STEADY);
    memcpy(atStart, buf, Draw::SIZE);
    WeatherView::render(buf, d, TempUnit::CELSIUS, "NYC",
                        WeatherView::PLACE_FLASH_MS + WeatherView::DESC_DWELL_MS - 50);
    memcpy(duringDwell, buf, Draw::SIZE);
    WeatherView::render(buf, d, TempUnit::CELSIUS, "NYC",
                        WeatherView::PLACE_FLASH_MS + WeatherView::DESC_DWELL_MS + 2000);
    memcpy(afterDwell, buf, Draw::SIZE);

    ASSERT(memcmp(atStart, duringDwell, Draw::SIZE) == 0,
           "description moved before the dwell elapsed");
    ASSERT(memcmp(atStart, afterDwell, Draw::SIZE) != 0,
           "description failed to scroll after the dwell");
    PASS();
}

void test_scroll_never_leaves_the_row_blank() {
    TEST("the scrolling description is never blank mid-wrap");
    // The seam between wrap repetitions is where a missing second copy shows up.
    const WeatherData d = reading(180, 99, true);   // longest description
    for (uint32_t t = STEADY; t < STEADY + 40000; t += 137) {
        WeatherView::render(buf, d, TempUnit::CELSIUS, "NYC", t);
        bool lit = false;
        for (uint8_t y = WeatherView::DESC_Y; y < WeatherView::DESC_Y + 7; y++)
            if (rowLit(buf, y)) { lit = true; break; }
        if (!lit) { ASSERT(false, "description row went blank during the scroll"); return; }
    }
    PASS();
}

void test_invalid_data_is_visibly_distinct() {
    TEST("a failed fetch is visibly distinct, not a stale reading");
    WeatherData bad;             // valid defaults to false
    bad.tempC10 = 999;
    bad.wmoCode = 0;             // would otherwise be CLEAR SKY

    uint8_t invalid[Draw::SIZE];
    WeatherView::render(buf, bad, TempUnit::CELSIUS, "NYC", STEADY);
    memcpy(invalid, buf, Draw::SIZE);

    WeatherView::render(buf, reading(999, 0, true), TempUnit::CELSIUS, "NYC", STEADY);

    ASSERT(memcmp(invalid, buf, Draw::SIZE) != 0,
           "invalid data rendered as though it were a clear-sky reading");
    ASSERT(anyLit(invalid), "invalid state rendered blank");
    PASS();
}

void test_unit_toggle_changes_the_reading() {
    TEST("switching units changes what is drawn");
    const WeatherData d = reading(200, 3, true);
    uint8_t c[Draw::SIZE];
    WeatherView::render(buf, d, TempUnit::CELSIUS, "NYC", STEADY);
    memcpy(c, buf, Draw::SIZE);
    WeatherView::render(buf, d, TempUnit::FAHRENHEIT, "NYC", STEADY);
    ASSERT(memcmp(c, buf, Draw::SIZE) != 0, "unit toggle had no visible effect");
    PASS();
}

void test_place_flash_then_reading() {
    TEST("place code flashes on entry, then yields to the reading");
    const WeatherData d = reading(120, 0, true);

    uint8_t early[Draw::SIZE], late[Draw::SIZE];
    WeatherView::render(buf, d, TempUnit::CELSIUS, "CAMB", 100);
    memcpy(early, buf, Draw::SIZE);
    WeatherView::render(buf, d, TempUnit::CELSIUS, "CAMB", STEADY);
    memcpy(late, buf, Draw::SIZE);

    ASSERT(memcmp(early, late, Draw::SIZE) != 0, "flash and steady state identical");
    ASSERT(anyLit(early), "place flash rendered blank");
    ASSERT(anyLit(late),  "steady state rendered blank");
    PASS();
}

void test_no_place_code_is_safe() {
    TEST("a null place code renders the reading immediately");
    // Without a flash there is no PLACE_FLASH_MS offset to subtract, which is
    // where an unsigned underflow would send the scroll offset to nonsense.
    const WeatherData d = reading(180, 2, true);
    for (uint32_t t = 0; t < 3000; t += 100) {
        WeatherView::render(buf, d, TempUnit::CELSIUS, nullptr, t);
        if (!anyLit(buf)) { ASSERT(false, "rendered blank with no place code"); return; }
        const uint8_t GAPS[] = { 4, 5, 6, 7, 15 };
        for (uint8_t y : GAPS)
            if (rowLit(buf, y)) { ASSERT(false, "content bled into a gap row"); return; }
    }
    PASS();
}

void test_every_place_code_renders() {
    TEST("every configured place renders its flash");
    const WeatherData d = reading(200, 3, true);
    for (uint8_t i = 0; i < Places::COUNT; i++) {
        WeatherView::render(buf, d, TempUnit::CELSIUS, Places::get(i).code, 100);
        ASSERT(anyLit(buf), "place code rendered blank");
    }
    PASS();
}

void test_places_table_bounds() {
    TEST("Places::get clamps out-of-range indices");
    ASSERT(Places::get(Places::COUNT).code == Places::get(Places::DEFAULT_INDEX).code,
           "out-of-range index should fall back to the default");
    ASSERT(Places::get(255).code == Places::get(Places::DEFAULT_INDEX).code,
           "255 should fall back to the default");
    PASS();
}

int main() {
    printf("=== WeatherView Unit Tests ===\n");
    test_every_code_has_a_description();
    test_undocumented_codes_are_unknown();
    test_descriptions_fit_the_scratch_buffer();
    test_longest_description_scrolls_fully_in_time();
    test_descriptions_are_renderable();
    test_clear_sky_reads_differently_at_night();
    test_intensity_is_distinguished();
    test_condition_colors_are_visible();
    test_condition_colors_separate_states();
    test_freezing_flag();
    test_celsius_rounding();
    test_fahrenheit_conversion();
    test_temperature_always_carries_a_degree_mark();
    test_formatted_temperatures_always_fit();
    test_format_strings();
    test_every_code_renders_something();
    test_layout_bands_are_respected();
    test_description_carries_the_condition_hue();
    test_description_dwells_then_scrolls();
    test_scroll_never_leaves_the_row_blank();
    test_invalid_data_is_visibly_distinct();
    test_unit_toggle_changes_the_reading();
    test_place_flash_then_reading();
    test_no_place_code_is_safe();
    test_every_place_code_renders();
    test_places_table_bounds();
    SUMMARY("WeatherView");
}
