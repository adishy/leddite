#pragma once

#include "Canvas.h"
#include "MarqueeEngine.h"
#include <stdint.h>
#include <time.h>

// TimeMode — Clock + Calendar display.
//
// CLOCK view:  static 24-hour display — HH centered on top row (y=1),
//              MM centered on bottom row (y=9). Redraws only on minute change.
//
// CALENDAR view: "23 MAY 2026" scrolling marquee in mint-green.
//
// Auto-switches every SWITCH_INTERVAL_MS (5s).
// Short press (from .ino): toggleDisplay() skips to the other view early.
//
// Call begin() on mode entry; update() every loop iteration.
// Caller must call marquee.stop() before switching away.
class TimeMode {
public:
    static const uint32_t SWITCH_INTERVAL_MS = 5000;
    static const uint16_t DATE_SPEED_PPS     = 30;  // px/sec for calendar scroll

    void begin(Canvas& canvas, MarqueeEngine& marquee);
    void update(Canvas& canvas, MarqueeEngine& marquee);
    void toggleDisplay(Canvas& canvas, MarqueeEngine& marquee);

private:
    void showClock(Canvas& canvas);                         // static 24h two-row display
    void showDate(Canvas& canvas, MarqueeEngine& marquee);  // start date marquee

    uint32_t lastSwitchMs = 0;
    bool     showingClock = true;
    int      lastMinute   = -1;

    // Date sprite buffer: "23 MAY 2026" = 11 chars × 6px × 7px × 3 bytes = 1386 bytes
    static const uint16_t DATE_BUF_CHARS = 12;
    uint8_t  dateBuf[DATE_BUF_CHARS * 6 * 7 * 3];
    uint16_t dateBufW = 0;

    static const char* const MONTH_ABBREVS[12];
};
