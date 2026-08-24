#pragma once

#include "Canvas.h"
#include "MarqueeEngine.h"
#include <stdint.h>
#include "WeatherView.h"
#include <time.h>

// TimeMode — Clock + Calendar + Weather, with DVD screensaver bounce.
//
// CLOCK view:    HH (sky-blue) over MM (pink), 12×15 px block.
//                Block bounces 1 px/sec (MAX_X=4, MAX_Y=1).
//
// CALENDAR view: DD (orange) over MMM-abbr (green), 18×15 px block.
//                Month shown as 3-char abbreviation (JAN..DEC).
//                18px > 16px canvas → x is pinned to 0, only vertical bounce.
//
// WEATHER view:  procedural condition icon over the temperature, rendered by
//                WeatherView. Static rather than bouncing — the icon reads
//                poorly in motion, and it flashes the place code on entry.
//                Data is supplied by the caller via setWeather(); this class
//                does no networking.
//
// Auto-switches every SWITCH_INTERVAL_MS (10 s), cycling clock → date → weather.
// Short press (from .ino): toggleDisplay() skips to the next view immediately.
//
// Call begin() on mode entry; update() every loop iteration.
// Caller must call marquee.stop() before switching away (kept for API compat).
class TimeMode {
public:
    static const uint32_t SWITCH_INTERVAL_MS = 10000;  // 10 s between clock↔date

    // Which face is showing. Weather is last so clock/date keep their order.
    enum class View : uint8_t { CLOCK = 0, CALENDAR = 1, WEATHER = 2, COUNT = 3 };

    void begin(Canvas& canvas, MarqueeEngine& marquee);
    void update(Canvas& canvas, MarqueeEngine& marquee);
    void toggleDisplay(Canvas& canvas, MarqueeEngine& marquee);

    // Supplied by the caller (WeatherClient + UiMode settings). Held by value so
    // TimeMode never touches the network or NVS.
    void setWeather(const WeatherData& d, TempUnit u, const char* code) {
        weather = d; unit = u; placeCode = code;
    }

private:
    static const uint8_t CLOCK_MAX_X = 4;   // clock (12px): 0..4, always fully on-screen
    static const int8_t  CAL_MIN_X   = -2;  // cal (18px): goes off left edge a little
    static const int8_t  CAL_MAX_X   =  2;  // cal (18px): goes off right edge a little
    static const uint8_t MAX_Y       = 1;   // 16 - 15

    void showFace(Canvas& canvas);  // render current view at dvdX/dvdY

    uint32_t lastSwitchMs = 0;
    View     view         = View::CLOCK;
    time_t   lastDrawSec  = 0;  // tracks per-second position updates
    uint32_t viewEnteredMs = 0; // drives the weather view's place-code flash

    // DVD screensaver position + velocity
    int8_t dvdX  = 2;   // start roughly centred
    int8_t dvdY  = 0;
    int8_t dvdDX = 1;
    int8_t dvdDY = 1;

    void showWeather(Canvas& canvas);

    // Reusable row render buffer — fits one 3-char row (18×7×3 = 378 bytes)
    uint8_t faceBuf[18 * 7 * 3];

    // Full-panel buffer for the weather view (WeatherView renders 16×16).
    uint8_t weatherBuf[16 * 16 * 3];

    WeatherData weather;
    TempUnit    unit      = TempUnit::CELSIUS;
    const char* placeCode = "";
};
