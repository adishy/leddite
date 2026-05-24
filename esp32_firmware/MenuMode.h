#pragma once

#include "Canvas.h"
#include "AppState.h"
#include <stdint.h>

// MenuMode — boot menu displayed after WiFi connects.
//
// Shows 4 options one at a time using 2-char abbreviations (12px wide, fits 16px canvas):
//   0: "CK" = Clock + Calendar   (cyan-blue)
//   1: "NT" = Network Canvas      (green)
//   2: "PT" = Pattern Slideshow   (magenta)
//   3: "TM" = Visual Timer        (amber)
//
// Bottom row: 4 indicator dots at x=3,6,9,12, y=14
//   active dot = bright white {255,255,255}
//   inactive dots = dim grey {30,30,30}
//
// Usage:
//   menuMode.begin(canvas)           — initialize, show first option
//   menuMode.onEncoderTurn(d, c)     — navigate and re-render immediately
//   menuMode.onEncoderPress(canvas)  — return chosen AppMode (caller transitions)
class MenuMode {
public:
    static const uint8_t NUM_OPTIONS = 4;

    void begin(Canvas& canvas);
    void onEncoderTurn(int delta, Canvas& canvas);  // navigate + re-render
    AppMode onEncoderPress(Canvas& canvas);          // return chosen AppMode

    void render(Canvas& canvas);                     // public for explicit re-draw

private:
    uint8_t currentOption = 0;

    // 2-char abbreviations for each option
    static const char* const LABELS[NUM_OPTIONS];

    // AppMode returned for each option index
    static const AppMode MODES[NUM_OPTIONS];

    // Colors for each option label (R, G, B)
    static const uint8_t COLORS[NUM_OPTIONS][3];

    // Dot x positions at y=DOT_Y
    static const uint8_t DOT_X[NUM_OPTIONS];
    static const uint8_t DOT_Y = 14;
};
