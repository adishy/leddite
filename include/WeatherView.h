#ifndef WEATHER_VIEW_H
#define WEATHER_VIEW_H

// WeatherView — renders a weather reading on the 16x16 panel.
//
// Pure C++/stdint only: it takes a plain WeatherData struct and knows nothing
// about HTTP, JSON or WiFi. The firmware's WeatherClient does the fetching and
// hands the result here, which keeps icon selection, unit conversion and layout
// unit-testable without a network.
//
// LAYOUT
//   y  0- 9   weather icon (drawn procedurally, not stored as bitmaps)
//   y 11-14   temperature, e.g. "12C" / "-5F" / "100F"
//
// No degree symbol: the row is 16px and SmallTextRenderer advances 4px per
// character, so "-12*C" would be 18px and scroll. Without it every realistic
// reading fits — "-40C" and "-60C" land at exactly 16px.
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
    // Icon shapes. Intensity (slight/moderate/heavy) is expressed as the number
    // of precipitation streaks rather than as separate icons, and freezing
    // variants reuse the rain/drizzle shape with an icy palette — which keeps
    // ~25 WMO codes down to 11 drawable shapes.
    enum Icon : uint8_t {
        CLEAR_DAY = 0,
        CLEAR_NIGHT,
        PARTLY_DAY,
        PARTLY_NIGHT,
        CLOUDY,
        FOG,
        DRIZZLE,
        RAIN,
        SNOW,
        THUNDER,
        UNKNOWN,
        ICON_COUNT,
    };

    static const uint16_t PLACE_FLASH_MS = 2000;

    // ── Pure mapping helpers (all unit-tested) ────────────────────────────────
    static Icon    iconFor(uint8_t wmoCode, bool isDay);
    static uint8_t precipStreaks(uint8_t wmoCode);   // 2 = slight .. 4 = heavy
    static bool    isFreezing(uint8_t wmoCode);      // freezing drizzle/rain

    // Whole degrees in the requested unit, rounded half away from zero.
    static int16_t displayTemp(int16_t tempC10, TempUnit unit);

    // Formats as "12C" / "-5F" / "100F" — never wider than 16px.
    static void    formatTemp(int16_t tempC10, TempUnit unit, char* out, size_t n);

    // ── Rendering ─────────────────────────────────────────────────────────────
    // `elapsedMs` is time since the view was entered, used for the place-code
    // flash and for animating precipitation.
    static void render(uint8_t* buf, const WeatherData& d, TempUnit unit,
                       const char* placeCode, uint32_t elapsedMs);

    // Draws just the icon into rows 0-9. Exposed for tests and the simulator.
    static void drawIcon(uint8_t* buf, Icon icon, uint8_t streaks,
                         bool freezing, uint32_t elapsedMs);

private:
    static void disc(uint8_t* buf, int16_t cx10, int16_t cy10, int16_t r10,
                     uint8_t r, uint8_t g, uint8_t b);
    static void cloud(uint8_t* buf, int16_t yOff, uint8_t r, uint8_t g, uint8_t b);
    static void cloudSmall(uint8_t* buf, int16_t yOff, uint8_t r, uint8_t g, uint8_t b);
    static void sun(uint8_t* buf, int16_t cx, int16_t cy, bool rays);
    static void moon(uint8_t* buf, int16_t cx, int16_t cy);
};

#endif // WEATHER_VIEW_H
