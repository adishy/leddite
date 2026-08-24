#ifndef BRIGHTNESS_MODEL_H
#define BRIGHTNESS_MODEL_H

// BrightnessModel — user-facing brightness levels and the power budget behind them.
//
// Pure C++/stdint only; the firmware applies the result via FastLED.
//
// WHY A TABLE AND NOT A FORMULA
// -----------------------------
// 256 WS2812B pixels at full white draw roughly 15 A, far beyond any supply this
// device runs on, so "level 10" cannot mean FastLED brightness 255. Levels map
// through an explicit table which is easy to audit and to unit-test, rather than
// through a curve someone has to re-derive.
//
// The table alone is not sufficient protection: it bounds the *scale* factor,
// not the *content*. A frame that happens to be all white still exceeds budget
// at high levels. The firmware therefore also calls
//   FastLED.setMaxPowerInVoltsAndMilliamps(SUPPLY_VOLTS, MAX_MILLIAMPS)
// which scales each frame down by what it actually contains. The two together
// mean no combination of level and content can brown out the ESP32.

#include <stdint.h>

class BrightnessModel {
public:
    static const uint8_t MIN_LEVEL     = 1;
    static const uint8_t MAX_LEVEL     = 10;
    static const uint8_t DEFAULT_LEVEL = 4;

    // Supply this device is built around (5V, 10A+ brick); budget leaves headroom.
    static const uint16_t SUPPLY_VOLTS  = 5;
    static const uint16_t MAX_MILLIAMPS = 8000;

    static const uint16_t NUM_LEDS         = 256;
    static const uint8_t  MA_PER_LED_WHITE = 60;   // WS2812B at full white

    // Clamps any int (including a corrupt NVS read) into [MIN_LEVEL, MAX_LEVEL].
    static uint8_t clampLevel(int level);

    // Level -> FastLED brightness (0-255).
    static uint8_t levelToFastLED(uint8_t level);

    // Current the panel would draw at this level with every pixel full white.
    // Above MAX_MILLIAMPS, FastLED's own limiter takes over at runtime.
    static uint32_t worstCaseMilliamps(uint8_t level);

    // Renders the editor screen into a 16*16*3 buffer: the level number over a
    // proportional bar. The panel's actual brightness changes as the level does,
    // so the screen is its own preview.
    static void renderScreen(uint8_t* buf, uint8_t level);

private:
    static const uint8_t LEVEL_TABLE[MAX_LEVEL];
};

#endif // BRIGHTNESS_MODEL_H
