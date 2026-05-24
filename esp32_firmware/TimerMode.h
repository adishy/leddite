#pragma once

#include "Canvas.h"
#include <stdint.h>

// TimerMode — visual countdown timer.
// Ported from esp32_visual_timer/led_rotary_enc_visual_timer.ino and adapted
// to use Canvas + TextRenderer instead of raw leds[] array.
//
// State machine (no IDLE — mode entry goes straight to SET_MINS):
//   SET_MINS  — encoder turn adjusts minutes (1–90), encoder press starts countdown
//   RUNNING   — progress bar fills canvas; encoder press cancels (returns to menu)
//   FINISHED  — blinking rainbow animation; encoder press returns to menu
//
// Return value of onEncoderPress():
//   true  → mode is done, caller should transition back to MENU
//   false → handled internally (timer started, etc.)

enum class TimerState { SET_MINS, RUNNING, FINISHED };

class TimerMode {
public:
    void begin(Canvas& canvas);
    void update(Canvas& canvas);            // call each loop()
    void onEncoderTurn(int delta);          // adjust minutes (SET_MINS only)
    bool onEncoderPress(Canvas& canvas);    // returns true if should exit to MENU

private:
    void showMinutes(Canvas& canvas);
    void showProgress(Canvas& canvas);
    void showFinished(Canvas& canvas);

    TimerState state           = TimerState::SET_MINS;
    int        selectedMinutes = 5;      // default 5 minutes
    uint32_t   startMs         = 0;
    uint32_t   durationMs      = 0;
    uint8_t    gHue            = 0;
    uint32_t   lastUpdateMs    = 0;      // frame rate limiter for animations
    uint32_t   lastBlinkMs     = 0;      // for FINISHED blink toggle
    bool       blinkOn         = true;

    static const uint8_t FRAME_MS = 33; // ~30 FPS for animations
};
