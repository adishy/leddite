#pragma once

#include "Canvas.h"
#include "AppState.h"
#include "MarqueeEngine.h"
#include "TextRenderer.h"
#include <stdint.h>

// MenuMode — boot menu displayed after WiFi connects.
//
// Shows full mode names scrolling left-to-right via MarqueeEngine (own instance):
//   0: "CLOCK + CALENDAR"  (golden amber)
//   1: "NETWORK CANVAS"    (warm orange)
//   2: "PATTERN SHOW"      (warm rose)
//   3: "VISUAL TIMER"      (warm yellow)
//   4: "OCTOPUS DANCE"     (ocean teal)
//
// Bottom indicator dots at x=2,5,8,11,14, y=14:
//   selected = mode accent colour; unselected = dim grey
//
// Usage:
//   menuMode.begin(canvas)             — init, start first name scroll
//   menuMode.onEncoderTurn(d, canvas)  — navigate + restart name scroll
//   menuMode.update(canvas)            — call every loop() to advance marquee
//   menuMode.onEncoderPress(canvas)    — return chosen AppMode
class MenuMode {
public:
    static const uint8_t  NUM_OPTIONS    = 5;
    static const uint16_t MARQUEE_SPEED  = 18;  // px/sec — comfortable reading pace

    void begin(Canvas& canvas);
    void onEncoderTurn(int delta, Canvas& canvas);
    void update(Canvas& canvas);           // advance marquee + draw frame; call every loop
    AppMode onEncoderPress(Canvas& canvas);

private:
    void startNameScroll();                // render name into nameBuf and start marquee
    void drawFrame(Canvas& canvas);        // draw current marquee frame + dots

    uint8_t  currentOption = 0;

    // Own MarqueeEngine — isolated from the global one used by TimeMode/NetworkMode
    MarqueeEngine menuMarquee;

    // Pixel buffer for the scrolling name sprite — 18 chars max × 6px × 7px × 3 bytes
    static const uint16_t NAME_BUF_CHARS = 18;
    uint8_t  nameBuf[NAME_BUF_CHARS * 6 * 7 * 3];
    uint16_t nameBufW = 0;

    // Full mode names
    static const char* const LABELS[NUM_OPTIONS];

    // AppMode returned for each option index
    static const AppMode MODES[NUM_OPTIONS];

    // Accent colors per mode [r, g, b]
    static const uint8_t COLORS[NUM_OPTIONS][3];

    // Dot x positions at y=DOT_Y  (5 dots: 2, 5, 8, 11, 14)
    static const uint8_t DOT_X[NUM_OPTIONS];
    static const uint8_t DOT_Y = 14;
};
