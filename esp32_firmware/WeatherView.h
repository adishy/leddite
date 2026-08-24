#ifndef WEATHER_VIEW_H
#define WEATHER_VIEW_H

// WeatherView — renders a weather reading on the 16x16 panel.
//
// Pure C++/stdint only: it takes a plain WeatherData struct and knows nothing
// about HTTP, JSON or WiFi. The firmware's WeatherClient does the fetching and
// hands the result here, which keeps description mapping, unit conversion and
// layout unit-testable without a network.
//
// LAYOUT
//   y  0- 3   temperature, small 3x4 font, e.g. "18*C"  ('*' is the degree mark)
//   y  5      1px rule in the condition colour
//   y  8-14   condition description, large 5x7 font, scrolling
//
// WHY NO ICONS
// ------------
// This view previously drew procedural sun/cloud/rain icons in rows 0-9. At
// 16x16 those shapes are near-unreadable and ambiguous between conditions
// ("partly cloudy" vs "overcast" differ by a couple of pixels), and they cost
// the whole top half of the panel. Words say it outright, and the freed space
// is what makes the large font affordable.
//
// THE DEGREE MARK
// ---------------
// SmallTextRenderer has no ASCII degree sign, so '*' is its designated degree
// glyph. "18*C" is 14px and fits, but "-12*C" is 18px and would overflow the
// row. There is no scroll fallback on the temperature — the reading must be
// legible at a glance — so formatTemp() drops the unit letter rather than the
// degree when the full string will not fit. See formatTemp().
//
// On entry the place code ("NYC", "CAMB") is shown for PLACE_FLASH_MS so it is
// always clear which location is on screen.
//
// Codes are WMO interpretation codes as returned by Open-Meteo's `weather_code`.

#include <stdint.h>
#include <stddef.h>

struct WeatherData {
    int16_t tempC10 = 0;      // temperature in tenths of a degree Celsius
    uint8_t wmoCode = 0;      // WMO interpretation code
    bool    isDay   = true;   // Open-Meteo `is_day`
    bool    valid   = false;  // false => never fetched, or the fetch failed
};

enum class TempUnit : uint8_t {
    CELSIUS    = 0,
    FAHRENHEIT = 1,
};

class WeatherView {
public:
    static const uint16_t PLACE_FLASH_MS = 2000;

    // Longest description the table may hold, in characters. TextRenderer's
    // stride is 6px, so this also fixes the scratch buffer at
    // DESC_MAX_CHARS * 6 * 7 * 3 bytes. Enforced by a unit test.
    static const uint8_t DESC_MAX_CHARS = 22;

    // Description scrolling, matching ListMenu's model: hold still long enough
    // to read the opening word, then scroll and wrap with a gap.
    //
    // DESC_PPS is set by a hard deadline, not by taste. TimeMode rotates
    // clock -> date -> weather every VIEW_BUDGET_MS, of which the place-code
    // flash consumes PLACE_FLASH_MS. The longest description must finish a full
    // pass inside what is left, or the tail of "SEVERE THUNDERSTORM" would never
    // be seen at all. 18 px/s is also what MenuMode scrolls at, so the reading
    // pace is consistent across the UI. A unit test enforces the deadline.
    static const uint16_t DESC_DWELL_MS = 900;
    static const uint8_t  DESC_PPS      = 18;   // px/sec
    static const uint8_t  DESC_GAP_PX   = 8;

    // How long this view stays on screen before TimeMode rotates away.
    // Mirrors TimeMode::SWITCH_INTERVAL_MS, which is Arduino-side and so cannot
    // be included here.
    static const uint16_t VIEW_BUDGET_MS = 10000;

    // Row geometry (see LAYOUT above).
    static const uint8_t TEMP_Y = 0;
    static const uint8_t RULE_Y = 5;
    static const uint8_t DESC_Y = 8;

    // ── Pure mapping helpers (all unit-tested) ────────────────────────────────

    // Human-readable condition, uppercase and restricted to the glyphs
    // TextRenderer actually has. `isDay` only distinguishes the clear-sky
    // wording; every other code reads the same day or night.
    static const char* describe(uint8_t wmoCode, bool isDay);

    // The accent colour for a condition — warm for sun, blue for rain, white
    // for snow, grey for fog. Both the temperature and the description are
    // tinted from this so the whole view reads as one state.
    static void conditionColor(uint8_t wmoCode, bool isDay,
                               uint8_t& r, uint8_t& g, uint8_t& b);

    static bool isFreezing(uint8_t wmoCode);      // freezing drizzle/rain

    // Whole degrees in the requested unit, rounded half away from zero.
    static int16_t displayTemp(int16_t tempC10, TempUnit unit);

    // Formats as "18*C" / "-5*C" / "212*F". Never wider than 16px: if the full
    // string would overflow, the unit letter is dropped ("-12*") rather than
    // the degree mark.
    static void    formatTemp(int16_t tempC10, TempUnit unit, char* out, size_t n);

    // ── Rendering ─────────────────────────────────────────────────────────────
    // `elapsedMs` is time since the view was entered, used for the place-code
    // flash and to drive the description scroll.
    static void render(uint8_t* buf, const WeatherData& d, TempUnit unit,
                       const char* placeCode, uint32_t elapsedMs);

private:
    // Horizontal offset for the scrolling description at a given elapsed time.
    static int16_t descScrollOffset(uint16_t descW, uint32_t elapsedMs);
};

#endif // WEATHER_VIEW_H
