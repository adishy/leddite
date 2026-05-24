#include "MenuMode.h"
#include <Arduino.h>
#include <string.h>

// ── Static data ───────────────────────────────────────────────────────────────

const char* const MenuMode::LABELS[NUM_OPTIONS] = {
    "CLOCK + CALENDAR",   // 16 chars × 6 = 96px wide
    "NETWORK CANVAS",     // 14 chars × 6 = 84px wide
    "PATTERN SHOW",       // 12 chars × 6 = 72px wide
    "VISUAL TIMER",       // 12 chars × 6 = 72px wide
};

const AppMode MenuMode::MODES[NUM_OPTIONS] = {
    AppMode::CLOCK_CAL,
    AppMode::NETWORK,
    AppMode::PATTERN,
    AppMode::TIMER,
};

// Distinct accent color per mode — warm palette
const uint8_t MenuMode::COLORS[NUM_OPTIONS][3] = {
    { 255, 200,  80},  // clock+cal:  golden amber
    { 255, 130,  40},  // network:    warm orange
    { 255,  90,  90},  // pattern:    warm rose/coral
    { 255, 230, 100},  // timer:      warm yellow
};

// 4 dots across 16px: x = 3, 6, 9, 12
const uint8_t MenuMode::DOT_X[NUM_OPTIONS] = {3, 6, 9, 12};

// ── Public API ────────────────────────────────────────────────────────────────

void MenuMode::begin(Canvas& canvas) {
    currentOption = 0;
    startNameScroll();
    drawFrame(canvas);
}

void MenuMode::onEncoderTurn(int delta, Canvas& canvas) {
    currentOption = (uint8_t)((currentOption + NUM_OPTIONS + delta) % NUM_OPTIONS);
    startNameScroll();
    drawFrame(canvas);  // immediate visual update on same loop tick
}

void MenuMode::update(Canvas& canvas) {
    if (menuMarquee.isActive()) {
        drawFrame(canvas);
    }
}

AppMode MenuMode::onEncoderPress(Canvas& canvas) {
    (void)canvas;
    menuMarquee.stop();
    return MODES[currentOption];
}

// ── Private ───────────────────────────────────────────────────────────────────

void MenuMode::startNameScroll() {
    const char*    label = LABELS[currentOption];
    const uint8_t* color = COLORS[currentOption];

    uint16_t h = 0;
    memset(nameBuf, 0, sizeof(nameBuf));
    TextRenderer::renderText(label, nameBuf, nameBufW, h, color);

    // Start marquee from x=16 (right edge of display) scrolling left
    menuMarquee.start(nameBuf, nameBufW, 7, MARQUEE_SPEED, millis());
}

void MenuMode::drawFrame(Canvas& canvas) {
    canvas.clear();

    // ── Scrolling name ────────────────────────────────────────────────────────
    // Vertically centered in the top 13 rows: y = (13 - 7) / 2 = 3
    if (menuMarquee.isActive() && nameBufW > 0) {
        int16_t xOff = menuMarquee.getXOffset(millis());
        canvas.drawSprite(nameBuf, (uint8_t)nameBufW, 7, (int8_t)xOff, 3, 0, false);
    }

    // ── Indicator dots ────────────────────────────────────────────────────────
    for (uint8_t i = 0; i < NUM_OPTIONS; i++) {
        uint8_t dot[3];
        if (i == currentOption) {
            // Active dot: use the mode's accent color (slightly dimmed)
            dot[0] = COLORS[i][0];
            dot[1] = COLORS[i][1];
            dot[2] = COLORS[i][2];
        } else {
            dot[0] = 25; dot[1] = 25; dot[2] = 25;  // dim grey
        }
        canvas.drawSprite(dot, 1, 1, (int8_t)DOT_X[i], (int8_t)DOT_Y, 0, false);
    }
}
