#include "TimerMode.h"
#include "TextRenderer.h"
#include <Arduino.h>
#include <FastLED.h>
#include <stdio.h>
#include <string.h>

// ── Public API ────────────────────────────────────────────────────────────────

void TimerMode::begin(Canvas& canvas) {
    state           = TimerState::SET_MINS;
    selectedMinutes = 5;   // default 5 minutes
    gHue            = 0;
    lastUpdateMs    = 0;
    blinkOn         = true;
    showMinutes(canvas);
}

void TimerMode::onEncoderTurn(int delta) {
    if (state != TimerState::SET_MINS) return;
    selectedMinutes += delta;
    if (selectedMinutes < 1)  selectedMinutes = 1;
    if (selectedMinutes > 90) selectedMinutes = 90;
    // showMinutes() is called from update() or directly from .ino after onEncoderTurn
    // To keep the interface simple, we re-render immediately via stored canvas ptr.
    // Since we can't call showMinutes() here without a Canvas& ref, the main .ino
    // calls timerMode.update(canvas) which triggers a re-render via lastUpdateMs=0.
    lastUpdateMs = 0; // force immediate re-render in update()
}

bool TimerMode::onEncoderPress(Canvas& canvas) {
    switch (state) {
        case TimerState::SET_MINS:
            // Start the countdown
            state      = TimerState::RUNNING;
            startMs    = millis();
            durationMs = (uint32_t)selectedMinutes * 60UL * 1000UL;
            canvas.clear();
            Serial.printf("[Timer] Started: %d min (%lu ms)\n", selectedMinutes, durationMs);
            return false;   // don't exit to menu

        case TimerState::RUNNING:
            // Cancel timer → back to menu
            Serial.println("[Timer] Cancelled");
            return true;

        case TimerState::FINISHED:
            // Dismissed → back to menu
            return true;
    }
    return false;
}

void TimerMode::update(Canvas& canvas) {
    uint32_t now = millis();

    switch (state) {
        case TimerState::SET_MINS:
            // Only re-render if lastUpdateMs was reset (by onEncoderTurn) or first call
            if (now - lastUpdateMs >= FRAME_MS || lastUpdateMs == 0) {
                lastUpdateMs = now;
                showMinutes(canvas);
            }
            break;

        case TimerState::RUNNING: {
            uint32_t elapsed = now - startMs;
            if (elapsed >= durationMs) {
                state = TimerState::FINISHED;
                lastBlinkMs = now;
                blinkOn     = true;
                Serial.println("[Timer] Finished!");
                showFinished(canvas);
            } else if (now - lastUpdateMs >= FRAME_MS) {
                lastUpdateMs = now;
                showProgress(canvas);
            }
            break;
        }

        case TimerState::FINISHED:
            // Blink at ~2 Hz (toggle every 300 ms)
            if (now - lastBlinkMs >= 300) {
                lastBlinkMs = now;
                blinkOn = !blinkOn;
                showFinished(canvas);
            }
            break;
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void TimerMode::showMinutes(Canvas& canvas) {
    canvas.clear();

    // Format minutes as 2-digit string with leading zero
    char numStr[3];
    snprintf(numStr, sizeof(numStr), "%02d", selectedMinutes);

    // Cornflower blue color
    const uint8_t color[3] = {100, 149, 237};

    // 2 chars = 12px wide, 7px tall
    // Center: x = (16-12)/2 = 2, y = (16-7)/2 = 4
    uint8_t  buf[12 * 7 * 3] = {0};
    uint16_t w = 0, h = 0;
    TextRenderer::renderText(numStr, buf, w, h, color);

    int8_t x = (int8_t)((16 - (int16_t)w) / 2);
    canvas.drawSprite(buf, (uint8_t)w, (uint8_t)h, x, 4, 0, false);
}

void TimerMode::showProgress(Canvas& canvas) {
    uint32_t now     = millis();
    uint32_t elapsed = now - startMs;

    // Calculate how many of the 256 pixels to light (proportional to elapsed time)
    uint32_t ledsToLight = (uint32_t)(((uint64_t)elapsed * 256) / durationMs);
    if (ledsToLight > 256) ledsToLight = 256;

    // Build a 16×16 pixel buffer
    // Pixel logical ordering: (x=0,y=0) = pixel 0, (x=15,y=0) = pixel 15,
    //                         (x=0,y=1) = pixel 16, etc.
    // Canvas.drawSprite() with the physical mapping in updateDisplay() handles HW layout.
    uint8_t pixBuf[16 * 16 * 3];
    memset(pixBuf, 0, sizeof(pixBuf));

    uint32_t count = 0;
    for (uint8_t y = 0; y < 16 && count < ledsToLight; y++) {
        for (uint8_t x = 0; x < 16 && count < ledsToLight; x++) {
            CRGB rgb;
            hsv2rgb_rainbow(CHSV(gHue, 255, 220), rgb);
            uint32_t idx = ((uint32_t)y * 16 + x) * 3;
            pixBuf[idx + 0] = rgb.r;
            pixBuf[idx + 1] = rgb.g;
            pixBuf[idx + 2] = rgb.b;
            count++;
        }
    }
    gHue++;  // slowly cycle hue each frame

    canvas.drawSprite(pixBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}

void TimerMode::showFinished(Canvas& canvas) {
    uint8_t pixBuf[16 * 16 * 3];

    if (blinkOn) {
        // Fill with cycling hue
        CRGB rgb;
        hsv2rgb_rainbow(CHSV(gHue, 255, 220), rgb);
        for (uint16_t i = 0; i < 256; i++) {
            pixBuf[i * 3 + 0] = rgb.r;
            pixBuf[i * 3 + 1] = rgb.g;
            pixBuf[i * 3 + 2] = rgb.b;
        }
        gHue += 8;  // faster hue cycling for "excited" finish state
    } else {
        // Off
        memset(pixBuf, 0, sizeof(pixBuf));
    }

    canvas.drawSprite(pixBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}
