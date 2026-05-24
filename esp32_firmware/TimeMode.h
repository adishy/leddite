#pragma once

#include "Canvas.h"
#include "MarqueeEngine.h"
#include <stdint.h>
#include <time.h>

// TimeMode — Clock + Calendar display with DVD screensaver bounce.
//
// CLOCK view:  HH (blue) over MM (pink), 12×15 px block.
//              Block bounces 1 px/sec around the 16×16 canvas (MAX_X=4, MAX_Y=1).
//
// CALENDAR view: DD (blue) over MM-number (pink), same bounce logic.
//
// Auto-switches every SWITCH_INTERVAL_MS (10 s).
// Short press (from .ino): toggleDisplay() skips to the other view immediately.
//
// Geometry:
//   FACE_W = textWidth("HH") = 12 px
//   FACE_H = 7 + 1 gap + 7   = 15 px
//   MAX_X  = 16 - 12          = 4
//   MAX_Y  = 16 - 15          = 1
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
    static const uint8_t FACE_W = 12;  // textWidth("HH")
    static const uint8_t FACE_H = 15;  // 7 + 1 + 7
    static const uint8_t MAX_X  = 4;   // 16 - FACE_W
    static const uint8_t MAX_Y  = 1;   // 16 - FACE_H

    void showFace(Canvas& canvas);  // render current view at dvdX/dvdY

    uint32_t lastSwitchMs = 0;
    bool     showingClock = true;
    time_t   lastDrawSec  = 0;  // tracks per-second position updates

    // DVD screensaver position + velocity
    int8_t dvdX  = 2;   // start roughly centred
    int8_t dvdY  = 0;
    int8_t dvdDX = 1;
    int8_t dvdDY = 1;

    // Reusable row render buffer — fits one 2-char row (12×7×3 = 252 bytes)
    uint8_t faceBuf[12 * 7 * 3];
};
