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

void TimeMode::begin(Canvas& canvas, MarqueeEngine& marquee) {
    marquee.stop();
    lastSwitchMs = millis();
    lastMinute   = -1;   // force immediate clock render
    showingClock = true;
    showClock(canvas);
}

void TimeMode::toggleDisplay(Canvas& canvas, MarqueeEngine& marquee) {
    showingClock = !showingClock;
    lastSwitchMs = millis();
    lastMinute   = -1;
    if (showingClock) {
        marquee.stop();
        showClock(canvas);
    } else {
        showDate(canvas, marquee);
    }
}

void TimeMode::update(Canvas& canvas, MarqueeEngine& marquee) {
    uint32_t now = millis();

    // Auto-switch every SWITCH_INTERVAL_MS
    if (now - lastSwitchMs >= SWITCH_INTERVAL_MS) {
        lastSwitchMs = now;
        showingClock = !showingClock;
        lastMinute   = -1;
        if (showingClock) {
            marquee.stop();
            showClock(canvas);
        } else {
            showDate(canvas, marquee);
        }
        return;
    }

    if (showingClock) {
        // Redraw only when the minute changes — avoids flicker
        struct tm timeinfo;
        if (getLocalTime(&timeinfo) && timeinfo.tm_min != lastMinute) {
            lastMinute = timeinfo.tm_min;
            showClock(canvas);
        }
    } else {
        // Drive marquee scroll each tick
        if (marquee.isActive() && dateBufW > 0) {
            int16_t xOff = marquee.getXOffset(now);
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

    // ── Hours row (y=1): 24-hour format, sky blue ────────────────────────────
    char hBuf[3];
    if (gotTime) snprintf(hBuf, sizeof(hBuf), "%02d", timeinfo.tm_hour);
    else         hBuf[0] = '-'; hBuf[1] = '-'; hBuf[2] = '\0';

    {
        const uint8_t color[3] = {80, 180, 255};  // sky blue (matches clock dot)
        uint8_t  buf[12 * 7 * 3] = {0};
        uint16_t w = 0, h = 0;
        TextRenderer::renderText(hBuf, buf, w, h, color);
        int8_t x = (int8_t)((16 - (int16_t)w) / 2);
        canvas.drawSprite(buf, (uint8_t)w, (uint8_t)h, x, 1, 0, false);
    }

    // ── Minutes row (y=9): same color ────────────────────────────────────────
    char mBuf[3];
    if (gotTime) snprintf(mBuf, sizeof(mBuf), "%02d", timeinfo.tm_min);
    else         mBuf[0] = '-'; mBuf[1] = '-'; mBuf[2] = '\0';

    {
        const uint8_t color[3] = {80, 180, 255};  // same sky blue
        uint8_t  buf[12 * 7 * 3] = {0};
        uint16_t w = 0, h = 0;
        TextRenderer::renderText(mBuf, buf, w, h, color);
        int8_t x = (int8_t)((16 - (int16_t)w) / 2);
        canvas.drawSprite(buf, (uint8_t)w, (uint8_t)h, x, 9, 0, false);
    }
}

void TimeMode::showDate(Canvas& canvas, MarqueeEngine& marquee) {
    // Build "DD MMM YYYY" — e.g. "23 MAY 2026"
    struct tm timeinfo;
    char dateStr[13] = "-- --- ----";
    if (getLocalTime(&timeinfo)) {
        snprintf(dateStr, sizeof(dateStr), "%02d %s %04d",
                 timeinfo.tm_mday,
                 MONTH_ABBREVS[timeinfo.tm_mon],
                 1900 + timeinfo.tm_year);
    }

    // Render into dateBuf (member — persists for scroll duration)
    const uint8_t color[3] = {80, 255, 120};  // mint green (matches network dot)
    uint16_t h = 0;
    memset(dateBuf, 0, sizeof(dateBuf));
    TextRenderer::renderText(dateStr, dateBuf, dateBufW, h, color);

    // Start marquee: scrolls from x=16 (right edge) to x=-dateBufW (off left)
    marquee.start(dateBuf, dateBufW, 7, DATE_SPEED_PPS, millis());

    // Initial frame at right edge
    canvas.clear();
    canvas.drawSprite(dateBuf, (uint8_t)dateBufW, 7, 16, 4, 0, false);
}
