#include "TimeMode.h"
#include "TextRenderer.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// ── Public API ────────────────────────────────────────────────────────────────

void TimeMode::begin(Canvas& canvas, MarqueeEngine& marquee) {
    marquee.stop();
    lastSwitchMs  = millis();
    lastDrawSec   = 0;          // force immediate draw
    view          = View::CLOCK;
    viewEnteredMs = millis();
    dvdX = 2; dvdY = 0;
    dvdDX = 1; dvdDY = 1;
    showFace(canvas);
}

void TimeMode::toggleDisplay(Canvas& canvas, MarqueeEngine& marquee) {
    marquee.stop();
    view          = (View)(((uint8_t)view + 1) % (uint8_t)View::COUNT);
    lastSwitchMs  = millis();
    viewEnteredMs = lastSwitchMs;
    lastDrawSec   = 0;          // force immediate redraw at new position
    showFace(canvas);
}

void TimeMode::update(Canvas& canvas, MarqueeEngine& marquee) {
    uint32_t now = millis();

    // Auto-switch every SWITCH_INTERVAL_MS
    if (now - lastSwitchMs >= SWITCH_INTERVAL_MS) {
        marquee.stop();
        lastSwitchMs  = now;
        view          = (View)(((uint8_t)view + 1) % (uint8_t)View::COUNT);
        viewEnteredMs = now;
        lastDrawSec   = 0;
        showFace(canvas);
        return;
    }

    // The weather face is static, so redraw it on the frame clock rather than
    // waiting for the next second tick — the place-code flash and the animated
    // precipitation both need sub-second updates.
    if (view == View::WEATHER) {
        showFace(canvas);
        return;
    }

    // Move + redraw once per second
    time_t nowSec = time(nullptr);
    if (nowSec == lastDrawSec) return;   // same second — nothing to do
    lastDrawSec = nowSec;

    // Advance DVD position — both views bounce X and Y.
    // Clock (12px) stays fully on-screen (0..CLOCK_MAX_X).
    // Calendar (18px) swings ±2 off each edge so both J and N get briefly clipped.
    dvdX += dvdDX;
    if (view == View::CLOCK) {
        if (dvdX <= 0)          { dvdX = 0;           dvdDX =  1; }
        if (dvdX >= CLOCK_MAX_X){ dvdX = CLOCK_MAX_X; dvdDX = -1; }
    } else {
        if (dvdX <= CAL_MIN_X)  { dvdX = CAL_MIN_X;   dvdDX =  1; }
        if (dvdX >= CAL_MAX_X)  { dvdX = CAL_MAX_X;   dvdDX = -1; }
    }
    dvdY += dvdDY;
    if (dvdY <= 0)    { dvdY = 0;    dvdDY =  1; }
    if (dvdY >= MAX_Y){ dvdY = MAX_Y; dvdDY = -1; }

    showFace(canvas);
}

// ── Private ───────────────────────────────────────────────────────────────────

void TimeMode::showFace(Canvas& canvas) {
    if (view == View::WEATHER) { showWeather(canvas); return; }

    canvas.clear();

    static const char* const MONTH_ABBR[12] = {
        "JAN","FEB","MAR","APR","MAY","JUN",
        "JUL","AUG","SEP","OCT","NOV","DEC"
    };

    struct tm timeinfo;
    bool gotTime = (time(nullptr) >= 1000000000UL) && getLocalTime(&timeinfo);

    char topBuf[3], botBuf[4];  // botBuf needs 4 bytes for 3-char abbr + null

    uint8_t topColor[3], botColor[3];

    if (view == View::CLOCK) {
        // Clock: HH (sky-blue) / MM (pink)
        if (gotTime) snprintf(topBuf, sizeof(topBuf), "%02d", timeinfo.tm_hour);
        else         { topBuf[0]='-'; topBuf[1]='-'; topBuf[2]='\0'; }

        if (gotTime) snprintf(botBuf, sizeof(botBuf), "%02d", timeinfo.tm_min);
        else         { botBuf[0]='-'; botBuf[1]='-'; botBuf[2]='\0'; }

        topColor[0]= 80; topColor[1]=180; topColor[2]=255;  // sky-blue
        botColor[0]=255; botColor[1]= 80; botColor[2]=160;  // pink
    } else {
        // Calendar: DD (orange) / MMM-abbr (green)
        if (gotTime) snprintf(topBuf, sizeof(topBuf), "%02d", timeinfo.tm_mday);
        else         { topBuf[0]='-'; topBuf[1]='-'; topBuf[2]='\0'; }

        if (gotTime) {
            int m = timeinfo.tm_mon;  // 0-based
            snprintf(botBuf, sizeof(botBuf), "%s", MONTH_ABBR[m < 0 ? 0 : m > 11 ? 11 : m]);
        } else {
            botBuf[0]='-'; botBuf[1]='-'; botBuf[2]='-'; botBuf[3]='\0';
        }

        topColor[0]=255; topColor[1]=160; topColor[2]= 50;  // warm orange
        botColor[0]= 80; botColor[1]=220; botColor[2]=120;  // soft green
    }

    uint16_t w = 0, h = 0;

    // Top row at dvdY
    memset(faceBuf, 0, sizeof(faceBuf));
    TextRenderer::renderText(topBuf, faceBuf, w, h, topColor);
    canvas.drawSprite(faceBuf, (uint8_t)w, (uint8_t)h, dvdX, dvdY, 0, false);

    // Bottom row at dvdY + 8  (7px glyph + 1px gap)
    memset(faceBuf, 0, sizeof(faceBuf));
    TextRenderer::renderText(botBuf, faceBuf, w, h, botColor);
    canvas.drawSprite(faceBuf, (uint8_t)w, (uint8_t)h, dvdX, (int8_t)(dvdY + 8), 0, false);
}

// ── Weather face ──────────────────────────────────────────────────────────────
//
// Rendering lives in WeatherView (src/), which is Arduino-free and unit-tested;
// this only supplies the data and pushes the result to the canvas.

void TimeMode::showWeather(Canvas& canvas) {
    const uint32_t elapsed = millis() - viewEnteredMs;
    WeatherView::render(weatherBuf, weather, unit, placeCode, elapsed);
    canvas.drawSprite(weatherBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}
