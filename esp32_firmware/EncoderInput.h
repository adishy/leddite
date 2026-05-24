#pragma once

#include <Arduino.h>       // defines HIGH, LOW, INPUT_PULLUP, etc.
#include <ESP32Encoder.h>
#include <stdint.h>

// EncoderEvent — returned by EncoderInput::poll() each loop iteration.
// Only one event fires per poll():
//   delta != 0          → encoder was turned (±1 per detent)
//   pressed == true     → short press (fires on button RELEASE if held < LONG_PRESS_MS)
//   longPress == true   → fires once while button is held >= LONG_PRESS_MS
//   released == true    → button released after a long press (no pressed event fires)
//
// Multiple fields being true simultaneously is possible only for delta + a button event
// if they happen in the same poll window (rare but handled).
struct EncoderEvent {
    int  delta;      // detent count change: +1 CW, -1 CCW, 0 = no turn
    bool pressed;    // short press (on release, after debounce, before LONG_PRESS_MS)
    bool released;   // released after a long press
    bool longPress;  // fired once while held >= LONG_PRESS_MS
};

// ── EncoderInput ──────────────────────────────────────────────────────────────
// Wraps ESP32Encoder with debounced button handling and long-press detection.
// Pins (matching legacy esp32_visual_timer firmware):
//   CLK = 32, DT = 33, SW = 25
// Button is INPUT_PULLUP: LOW = pressed.
class EncoderInput {
public:
    static const uint8_t  CLK_PIN       = 32;
    static const uint8_t  DT_PIN        = 33;
    static const uint8_t  SW_PIN        = 25;
    static const uint16_t DEBOUNCE_MS   = 100;
    static const uint16_t LONG_PRESS_MS = 3000;  // 3s hold → back/home

    void begin();

    // Call once per loop(). Returns an event struct.
    // Returns {0, false, false, false} if nothing happened.
    EncoderEvent poll();

private:
    ESP32Encoder enc;
    long    lastDetentPos   = 0;
    int     lastButtonState = HIGH;
    uint32_t pressStartMs   = 0;
    bool    longFired       = false;
};
