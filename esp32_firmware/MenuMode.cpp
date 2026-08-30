#include "MenuMode.h"
#include <Arduino.h>
#include <string.h>

// ── Static data ───────────────────────────────────────────────────────────────

const char* const MenuMode::LABELS[NUM_OPTIONS] = {
    "CLOCK CAL WEATHER",  // 17 chars × 6 = 102px wide
    "NETWORK CANVAS",     // 14 chars × 6 = 84px wide
    "TIMER",              //  5 chars × 6 = 30px wide
    "CHARACTERS",         // 10 chars × 6 = 60px wide
    "GAME SCREENSAVERS",  // 17 chars × 6 = 102px wide
    "SETTINGS",           //  8 chars × 6 = 48px wide
};

const AppMode MenuMode::MODES[NUM_OPTIONS] = {
    AppMode::CLOCK_CAL,
    AppMode::NETWORK,
    AppMode::TIMER,
    AppMode::OCTOPUS,
    AppMode::GAMES,
    AppMode::SETTINGS,
};

// Distinct accent color per mode — warm palette + ocean teal for characters
const uint8_t MenuMode::COLORS[NUM_OPTIONS][3] = {
    { 255, 200,  80},  // clock+cal+weather: golden amber
    { 255, 130,  40},  // network:           warm orange
    { 255, 230, 100},  // timer:             warm yellow
    {  40, 220, 210},  // characters:        ocean teal
    { 130, 230, 120},  // game screensavers: arcade green
    { 190, 150, 255},  // settings:          soft violet
};

// 6 dots at a 2px pitch, centred: x = 3..13 spans 10 of the 16 px.
const uint8_t MenuMode::DOT_X[NUM_OPTIONS] = {3, 5, 7, 9, 11, 13};

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
            // Active dot: use the mode's accent color
            dot[0] = COLORS[i][0];
            dot[1] = COLORS[i][1];
            dot[2] = COLORS[i][2];
        } else {
            dot[0] = 25; dot[1] = 25; dot[2] = 25;  // dim grey
        }
        canvas.drawSprite(dot, 1, 1, (int8_t)DOT_X[i], (int8_t)DOT_Y, 0, false);
    }
}
