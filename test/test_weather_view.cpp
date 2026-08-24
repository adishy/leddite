#include "WeatherView.h"
#include "SmallTextRenderer.h"
#include "Places.h"
#include "Draw.h"
#include "test_harness.h"
#include <string.h>

static uint8_t buf[Draw::SIZE];

static bool anyLit(const uint8_t* b) {
    for (uint16_t i = 0; i < Draw::SIZE; i++) if (b[i]) return true;
    return false;
}

// ── WMO code mapping ──────────────────────────────────────────────────────────

void test_icon_mapping_clear_and_cloud() {
    TEST("clear/partly/overcast map by code and is_day");
    ASSERT(WeatherView::iconFor(0, true)  == WeatherView::CLEAR_DAY,    "0 day");
    ASSERT(WeatherView::iconFor(0, false) == WeatherView::CLEAR_NIGHT,  "0 night");
    ASSERT(WeatherView::iconFor(1, true)  == WeatherView::CLEAR_DAY,    "1 day");
    ASSERT(WeatherView::iconFor(1, false) == WeatherView::CLEAR_NIGHT,  "1 night");
    ASSERT(WeatherView::iconFor(2, true)  == WeatherView::PARTLY_DAY,   "2 day");
    ASSERT(WeatherView::iconFor(2, false) == WeatherView::PARTLY_NIGHT, "2 night");
    ASSERT(WeatherView::iconFor(3, true)  == WeatherView::CLOUDY,       "3 overcast");
    ASSERT(WeatherView::iconFor(3, false) == WeatherView::CLOUDY,       "3 is day-agnostic");
    PASS();
}

void test_icon_mapping_precipitation() {
    TEST("fog/drizzle/rain/snow/thunder ranges");
    ASSERT(WeatherView::iconFor(45, true) == WeatherView::FOG, "45 fog");
    ASSERT(WeatherView::iconFor(48, true) == WeatherView::FOG, "48 rime fog");

    for (uint8_t c = 51; c <= 57; c++)
        ASSERT(WeatherView::iconFor(c, true) == WeatherView::DRIZZLE, "51-57 drizzle");
    for (uint8_t c = 61; c <= 67; c++)
        ASSERT(WeatherView::iconFor(c, true) == WeatherView::RAIN, "61-67 rain");
    for (uint8_t c = 71; c <= 77; c++)
        ASSERT(WeatherView::iconFor(c, true) == WeatherView::SNOW, "71-77 snow");
    for (uint8_t c = 80; c <= 82; c++)
        ASSERT(WeatherView::iconFor(c, true) == WeatherView::RAIN, "80-82 rain showers");

    ASSERT(WeatherView::iconFor(85, true) == WeatherView::SNOW, "85 snow showers");
    ASSERT(WeatherView::iconFor(86, true) == WeatherView::SNOW, "86 snow showers");
    ASSERT(WeatherView::iconFor(95, true) == WeatherView::THUNDER, "95 thunder");
    ASSERT(WeatherView::iconFor(96, true) == WeatherView::THUNDER, "96 thunder+hail");
    ASSERT(WeatherView::iconFor(99, true) == WeatherView::THUNDER, "99 thunder+hail");
    PASS();
}

void test_unknown_codes_fall_back() {
    TEST("undefined codes fall back to UNKNOWN");
    const uint8_t BAD[] = { 4, 10, 30, 44, 49, 50, 58, 60, 68, 70, 78, 79, 83, 84, 87, 94, 100, 200, 255 };
    for (uint8_t c : BAD)
        ASSERT(WeatherView::iconFor(c, true) == WeatherView::UNKNOWN, "should be UNKNOWN");
    PASS();
}

void test_intensity_and_freezing() {
    TEST("intensity becomes streak count; freezing is a palette flag");
    ASSERT_EQ(WeatherView::precipStreaks(61), 2, "slight rain");
    ASSERT_EQ(WeatherView::precipStreaks(63), 3, "moderate rain");
    ASSERT_EQ(WeatherView::precipStreaks(65), 4, "heavy rain");
    ASSERT_EQ(WeatherView::precipStreaks(71), 2, "slight snow");
    ASSERT_EQ(WeatherView::precipStreaks(75), 4, "heavy snow");

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

void test_formatted_temperatures_always_fit() {
    TEST("formatted temperature always fits a 16px row");
    // The layout has no scroll fallback on the temperature row, so this is a
    // hard constraint, not a preference.
    for (int16_t c10 = -700; c10 <= 600; c10 = (int16_t)(c10 + 7)) {
        for (uint8_t u = 0; u < 2; u++) {
            char s[8];
            WeatherView::formatTemp(c10, (TempUnit)u, s, sizeof(s));
            const uint16_t w = SmallTextRenderer::textWidth(s);
            if (w > 16) { ASSERT(false, "temperature string overflows the row"); return; }
        }
    }
    PASS();
}

void test_format_strings() {
    TEST("formatTemp output strings");
    char s[8];
    WeatherView::formatTemp(123,  TempUnit::CELSIUS,    s, sizeof(s));
    ASSERT(strcmp(s, "12C") == 0, "12.3C -> 12C");
    WeatherView::formatTemp(-52,  TempUnit::CELSIUS,    s, sizeof(s));
    ASSERT(strcmp(s, "-5C") == 0, "-5.2C -> -5C");
    WeatherView::formatTemp(1000, TempUnit::FAHRENHEIT, s, sizeof(s));
    ASSERT(strcmp(s, "212F") == 0, "100C -> 212F");
    PASS();
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void test_all_icons_render_something() {
    TEST("every icon draws visible pixels");
    for (uint8_t i = 0; i < WeatherView::ICON_COUNT; i++) {
        Draw::clear(buf);
        WeatherView::drawIcon(buf, (WeatherView::Icon)i, 3, false, 500);
        ASSERT(anyLit(buf), "icon rendered blank");
    }
    PASS();
}

void test_icons_stay_in_their_band() {
    TEST("icons stay clear of the temperature row");
    for (uint8_t i = 0; i < WeatherView::ICON_COUNT; i++) {
        for (uint32_t t = 0; t < 1200; t += 160) {
            Draw::clear(buf);
            WeatherView::drawIcon(buf, (WeatherView::Icon)i, 4, false, t);
            for (uint8_t y = 11; y < 16; y++)
                for (uint8_t x = 0; x < 16; x++) {
                    const uint16_t k = (uint16_t)(y * 16 + x) * 3;
                    if (buf[k] | buf[k + 1] | buf[k + 2]) {
                        ASSERT(false, "icon bled into the temperature rows");
                        return;
                    }
                }
        }
    }
    PASS();
}

void test_invalid_data_shows_unknown() {
    TEST("a failed fetch is visibly distinct, not a stale reading");
    WeatherData d;              // valid defaults to false
    d.tempC10 = 999;
    d.wmoCode = 0;              // would otherwise be CLEAR_DAY
    WeatherView::render(buf, d, TempUnit::CELSIUS, "NYC", 5000);

    uint8_t clear[Draw::SIZE];
    Draw::clear(clear);
    WeatherView::drawIcon(clear, WeatherView::CLEAR_DAY, 3, false, 5000);
    ASSERT(memcmp(buf, clear, Draw::SIZE) != 0,
           "invalid data rendered as though it were a clear-sky reading");
    ASSERT(anyLit(buf), "invalid state rendered blank");
    PASS();
}

void test_place_flash_then_icon() {
    TEST("place code flashes on entry, then yields to the icon");
    WeatherData d;
    d.valid = true; d.tempC10 = 120; d.wmoCode = 0; d.isDay = true;

    uint8_t early[Draw::SIZE], late[Draw::SIZE];
    WeatherView::render(buf, d, TempUnit::CELSIUS, "CAMB", 100);
    memcpy(early, buf, Draw::SIZE);
    WeatherView::render(buf, d, TempUnit::CELSIUS, "CAMB",
                        WeatherView::PLACE_FLASH_MS + 100);
    memcpy(late, buf, Draw::SIZE);

    ASSERT(memcmp(early, late, Draw::SIZE) != 0, "flash and steady state identical");
    ASSERT(anyLit(early), "place flash rendered blank");
    ASSERT(anyLit(late),  "steady state rendered blank");
    PASS();
}

void test_every_place_code_renders() {
    TEST("every configured place renders its flash");
    WeatherData d;
    d.valid = true; d.tempC10 = 200; d.wmoCode = 3;
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
    test_icon_mapping_clear_and_cloud();
    test_icon_mapping_precipitation();
    test_unknown_codes_fall_back();
    test_intensity_and_freezing();
    test_celsius_rounding();
    test_fahrenheit_conversion();
    test_formatted_temperatures_always_fit();
    test_format_strings();
    test_all_icons_render_something();
    test_icons_stay_in_their_band();
    test_invalid_data_shows_unknown();
    test_place_flash_then_icon();
    test_every_place_code_renders();
    test_places_table_bounds();
    SUMMARY("WeatherView");
}
