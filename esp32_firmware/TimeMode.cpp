#include "TimeMode.h"
#include "TextRenderer.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// ── Public API ────────────────────────────────────────────────────────────────

void TimeMode::begin(Canvas& canvas, MarqueeEngine& marquee) {
    marquee.stop();
    lastSwitchMs = millis();
    lastDrawSec  = 0;          // force immediate draw
    showingClock = true;
    dvdX = 2; dvdY = 0;
    dvdDX = 1; dvdDY = 1;
    showFace(canvas);
}

void TimeMode::toggleDisplay(Canvas& canvas, MarqueeEngine& marquee) {
    marquee.stop();
    showingClock = !showingClock;
    lastSwitchMs = millis();
    lastDrawSec  = 0;          // force immediate redraw at new position
    showFace(canvas);
}

void TimeMode::update(Canvas& canvas, MarqueeEngine& marquee) {
    uint32_t now = millis();

    // Auto-switch every SWITCH_INTERVAL_MS
    if (now - lastSwitchMs >= SWITCH_INTERVAL_MS) {
        marquee.stop();
        lastSwitchMs = now;
        showingClock = !showingClock;
        lastDrawSec  = 0;
        showFace(canvas);
        return;
    }

    // Move + redraw once per second
    time_t nowSec = time(nullptr);
    if (nowSec == lastDrawSec) return;   // same second — nothing to do
    lastDrawSec = nowSec;

    // Advance DVD position
    dvdX += dvdDX;
    dvdY += dvdDY;

    // Bounce off walls
    if (dvdX <= 0)    { dvdX = 0;    dvdDX =  1; }
    if (dvdX >= MAX_X){ dvdX = MAX_X; dvdDX = -1; }
    if (dvdY <= 0)    { dvdY = 0;    dvdDY =  1; }
    if (dvdY >= MAX_Y){ dvdY = MAX_Y; dvdDY = -1; }

    showFace(canvas);
}

// ── Private ───────────────────────────────────────────────────────────────────

void TimeMode::showFace(Canvas& canvas) {
    canvas.clear();

    struct tm timeinfo;
    bool gotTime = (time(nullptr) >= 1000000000UL) && getLocalTime(&timeinfo);

    char topBuf[3], botBuf[3];

    if (showingClock) {
        // Top row: HH  (sky blue)
        if (gotTime) snprintf(topBuf, 3, "%02d", timeinfo.tm_hour);
        else         { topBuf[0]='-'; topBuf[1]='-'; topBuf[2]='\0'; }

        // Bottom row: MM  (pink)
        if (gotTime) snprintf(botBuf, 3, "%02d", timeinfo.tm_min);
        else         { botBuf[0]='-'; botBuf[1]='-'; botBuf[2]='\0'; }
    } else {
        // Top row: DD  (sky blue)
        if (gotTime) snprintf(topBuf, 3, "%02d", timeinfo.tm_mday);
        else         { topBuf[0]='-'; topBuf[1]='-'; topBuf[2]='\0'; }

        // Bottom row: month number 01-12  (pink)
        if (gotTime) snprintf(botBuf, 3, "%02d", timeinfo.tm_mon + 1);
        else         { botBuf[0]='-'; botBuf[1]='-'; botBuf[2]='\0'; }
    }

    const uint8_t BLUE[3] = {80, 180, 255};
    const uint8_t PINK[3] = {255, 80, 160};

    uint16_t w = 0, h = 0;

    // Top row at dvdY
    memset(faceBuf, 0, sizeof(faceBuf));
    TextRenderer::renderText(topBuf, faceBuf, w, h, BLUE);
    canvas.drawSprite(faceBuf, (uint8_t)w, (uint8_t)h, dvdX, dvdY, 0, false);

    // Bottom row at dvdY + 8  (7px glyph + 1px gap)
    memset(faceBuf, 0, sizeof(faceBuf));
    TextRenderer::renderText(botBuf, faceBuf, w, h, PINK);
    canvas.drawSprite(faceBuf, (uint8_t)w, (uint8_t)h, dvdX, (int8_t)(dvdY + 8), 0, false);
}
