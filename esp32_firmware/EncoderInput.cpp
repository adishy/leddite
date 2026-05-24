#include "EncoderInput.h"
#include <Arduino.h>

void EncoderInput::begin() {
    // Button pin: active LOW, internal pull-up
    pinMode(SW_PIN, INPUT_PULLUP);

    // Attach full quadrature — DT pin first, CLK pin second (matches legacy visual timer)
    enc.attachFullQuad(DT_PIN, CLK_PIN);
    enc.clearCount();
    lastDetentPos   = 0;
    lastButtonState = HIGH;
    longFired       = false;
}

EncoderEvent EncoderInput::poll() {
    EncoderEvent ev = {0, false, false, false};

    // ── Encoder rotation ─────────────────────────────────────────────────────
    // Full quadrature gives 4 counts per physical detent; divide by 4.
    long rawCount   = enc.getCount();
    long detentPos  = rawCount / 4;
    long detentDiff = detentPos - lastDetentPos;

    if (detentDiff != 0) {
        // Clamp to ±1 per poll to handle fast spins gracefully
        ev.delta = (detentDiff > 0) ? 1 : -1;
        // Re-align count to the nearest valid detent boundary
        lastDetentPos = detentPos;
        enc.setCount(detentPos * 4);
    }

    // ── Button ───────────────────────────────────────────────────────────────
    int currentState = digitalRead(SW_PIN);
    uint32_t now     = millis();

    if (lastButtonState == HIGH && currentState == LOW) {
        // Falling edge: button pressed down (with debounce check)
        if (now - pressStartMs > DEBOUNCE_MS) {
            pressStartMs = now;
            longFired    = false;
        }
    } else if (lastButtonState == LOW && currentState == HIGH) {
        // Rising edge: button released
        if ((now - pressStartMs) >= DEBOUNCE_MS) {
            if (!longFired) {
                ev.pressed = true;   // short press
            } else {
                ev.released = true;  // released after long press
            }
            longFired = false;
        }
    } else if (lastButtonState == LOW && currentState == LOW) {
        // Still held: check for long press threshold
        if (!longFired && (now - pressStartMs) >= LONG_PRESS_MS) {
            ev.longPress = true;
            longFired    = true;
        }
    }

    lastButtonState = currentState;
    return ev;
}
