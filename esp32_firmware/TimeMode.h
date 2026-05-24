#pragma once

#include "Canvas.h"
#include "MarqueeEngine.h"
#include <stdint.h>
#include <time.h>

// TimeMode — Clock + Calendar display.
//
// Alternates every SWITCH_INTERVAL_MS (5s) between:
//   CLOCK:    Two rows — hours (y=1) and minutes (y=9) using TextRenderer 5×7 font
//             12-hour format. If NTP not synced, displays "--" on each row.
//   CALENDAR: Scrolling marquee of "DD MMM" (e.g. "23 MAY") via MarqueeEngine.
//
// Call begin() once on mode entry, update() every loop iteration.
// Caller must call marquee.stop() before switching away to prevent stale state.
class TimeMode {
public:
    static const uint32_t SWITCH_INTERVAL_MS = 5000;
    static const uint16_t MARQUEE_SPEED_PPS  = 30;  // pixels per second for date scroll

    void begin(Canvas& canvas, MarqueeEngine& marquee);
    void update(Canvas& canvas, MarqueeEngine& marquee);

    // Short press while in Clock+Calendar mode: skip to the other display immediately
    void toggleDisplay(Canvas& canvas, MarqueeEngine& marquee);

private:
    void showClock(Canvas& canvas);
    void showDate(Canvas& canvas, MarqueeEngine& marquee);

    uint32_t lastSwitchMs  = 0;
    bool     showingClock  = true;
    int      lastMinute    = -1;  // avoid redrawing clock every loop tick

    // Pre-allocated date sprite buffer: "DD MMM" = 6 chars × 6px × 7px × 3 bytes = 756
    uint8_t  dateBuf[6 * 6 * 7 * 3];
    uint16_t dateBufW = 0;

    static const char* const MONTH_ABBREVS[12];
};
