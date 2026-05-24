#pragma once

#include "Canvas.h"
#include "MarqueeEngine.h"
#include <stdint.h>
#include <time.h>

// TimeMode — Clock + Calendar display with DVD screensaver bounce.
//
// CLOCK view:    HH (sky-blue) over MM (pink), 12×15 px block.
//                Block bounces 1 px/sec (MAX_X=4, MAX_Y=1).
//
// CALENDAR view: DD (orange) over MMM-abbr (green), 18×15 px block.
//                Month shown as 3-char abbreviation (JAN..DEC).
//                18px > 16px canvas → x is pinned to 0, only vertical bounce.
//
// Auto-switches every SWITCH_INTERVAL_MS (10 s).
// Short press (from .ino): toggleDisplay() skips to the other view immediately.
//
// Call begin() on mode entry; update() every loop iteration.
// Caller must call marquee.stop() before switching away (kept for API compat).
class TimeMode {
public:
    static const uint32_t SWITCH_INTERVAL_MS = 10000;  // 10 s between clock↔date

    void begin(Canvas& canvas, MarqueeEngine& marquee);
    void update(Canvas& canvas, MarqueeEngine& marquee);
    void toggleDisplay(Canvas& canvas, MarqueeEngine& marquee);

private:
    static const uint8_t CLOCK_MAX_X = 4;   // clock (12px): 0..4, always fully on-screen
    static const int8_t  CAL_MIN_X   = -2;  // cal (18px): goes off left edge a little
    static const int8_t  CAL_MAX_X   =  2;  // cal (18px): goes off right edge a little
    static const uint8_t MAX_Y       = 1;   // 16 - 15

    void showFace(Canvas& canvas);  // render current view at dvdX/dvdY

    uint32_t lastSwitchMs = 0;
    bool     showingClock = true;
    time_t   lastDrawSec  = 0;  // tracks per-second position updates

    // DVD screensaver position + velocity
    int8_t dvdX  = 2;   // start roughly centred
    int8_t dvdY  = 0;
    int8_t dvdDX = 1;
    int8_t dvdDY = 1;

    // Reusable row render buffer — fits one 3-char row (18×7×3 = 378 bytes)
    uint8_t faceBuf[18 * 7 * 3];
};
