#include "TimeMode.h"
#include "TextRenderer.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

const char* const TimeMode::MONTH_ABBREVS[12] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

// ── Public API ────────────────────────────────────────────────────────────────

void TimeMode::toggleDisplay(Canvas& canvas, MarqueeEngine& marquee) {
    // Manually switch between clock and date, resetting the auto-switch timer
    showingClock = !showingClock;
    lastSwitchMs = millis();
    lastMinute   = -1;  // force clock redraw
    if (showingClock) {
        marquee.stop();
        showClock(canvas);
    } else {
        showDate(canvas, marquee);
    }
}

void TimeMode::begin(Canvas& canvas, MarqueeEngine& marquee) {
    marquee.stop();
    lastSwitchMs = millis();
    lastMinute   = -1;   // force immediate render
    showingClock = true;
    showClock(canvas);
}

void TimeMode::update(Canvas& canvas, MarqueeEngine& marquee) {
    uint32_t now = millis();

    // Auto-switch between clock and date every SWITCH_INTERVAL_MS
    if (now - lastSwitchMs >= SWITCH_INTERVAL_MS) {
        lastSwitchMs = now;
        showingClock = !showingClock;
        lastMinute   = -1;  // force clock redraw when we switch back to it
        if (showingClock) {
            marquee.stop();
            showClock(canvas);
        } else {
            showDate(canvas, marquee);
        }
        return;
    }

    if (showingClock) {
        // Redraw only when the minute changes to avoid flickering
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            if (timeinfo.tm_min != lastMinute) {
                lastMinute = timeinfo.tm_min;
                showClock(canvas);
            }
        }
    } else {
        // Date: drive the marquee scroll each tick (if active)
        if (marquee.isActive() && dateBufW > 0) {
            int16_t xOff = marquee.getXOffset(now);
            // Vertical centering: 7px tall in 16px canvas → y = (16-7)/2 = 4
            canvas.drawSprite(dateBuf, (uint8_t)dateBufW, 7, (int8_t)xOff, 4, 0,
                              /*clearBefore=*/true);
        }
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void TimeMode::showClock(Canvas& canvas) {
    canvas.clear();

    struct tm timeinfo;
    bool gotTime = getLocalTime(&timeinfo);

    // ── Hours row (y=1) ──────────────────────────────────────────────────────
    char hBuf[3] = "--";
    if (gotTime) {
        int h12 = timeinfo.tm_hour % 12;
        if (h12 == 0) h12 = 12;
        snprintf(hBuf, sizeof(hBuf), "%2d", h12);
        // Replace leading space with '0' for consistent 2-char width
        if (hBuf[0] == ' ') hBuf[0] = '0';
    }
    {
        const uint8_t color[3] = {120, 200, 255};  // soft blue
        uint8_t  buf[12 * 7 * 3] = {0};
        uint16_t w = 0, h = 0;
        TextRenderer::renderText(hBuf, buf, w, h, color);
        int8_t x = (int8_t)((16 - (int16_t)w) / 2);
        canvas.drawSprite(buf, (uint8_t)w, (uint8_t)h, x, 1, 0, false);
    }

    // ── Minutes row (y=9) ────────────────────────────────────────────────────
    char mBuf[3] = "--";
    if (gotTime) snprintf(mBuf, sizeof(mBuf), "%02d", timeinfo.tm_min);
    {
        const uint8_t color[3] = {255, 200, 80};  // amber
        uint8_t  buf[12 * 7 * 3] = {0};
        uint16_t w = 0, h = 0;
        TextRenderer::renderText(mBuf, buf, w, h, color);
        int8_t x = (int8_t)((16 - (int16_t)w) / 2);
        canvas.drawSprite(buf, (uint8_t)w, (uint8_t)h, x, 9, 0, false);
    }
}

void TimeMode::showDate(Canvas& canvas, MarqueeEngine& marquee) {
    // Build "DD MMM" string, e.g. "23 MAY"
    struct tm timeinfo;
    char dateStr[8] = "-- ---";
    if (getLocalTime(&timeinfo)) {
        snprintf(dateStr, sizeof(dateStr), "%02d %s",
                 timeinfo.tm_mday,
                 MONTH_ABBREVS[timeinfo.tm_mon]);
    }

    // Render into dateBuf (member, persists for scroll duration)
    const uint8_t color[3] = {100, 255, 180};  // mint green
    uint16_t h = 0;
    TextRenderer::renderText(dateStr, dateBuf, dateBufW, h, color);

    // Start marquee scrolling (scrolls left from x=16 to x=-dateBufW)
    marquee.start(dateBuf, dateBufW, 7, MARQUEE_SPEED_PPS, millis());

    // Initial frame: position at right edge
    canvas.clear();
    canvas.drawSprite(dateBuf, (uint8_t)dateBufW, 7, 16, 4, 0, false);
}
