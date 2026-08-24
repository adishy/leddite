#include "BrightnessModel.h"
#include "Draw.h"
#include "SmallTextRenderer.h"
#include <stdio.h>

// Level 1 stays usable in a dark room; level 10 is the highest value that keeps
// typical (non-white-field) content inside the supply budget without leaning on
// FastLED's frame limiter for every frame.
const uint8_t BrightnessModel::LEVEL_TABLE[BrightnessModel::MAX_LEVEL] = {
    20, 32, 48, 66, 86, 108, 132, 156, 178, 200
};

uint8_t BrightnessModel::clampLevel(int level) {
    if (level < (int)MIN_LEVEL) return MIN_LEVEL;
    if (level > (int)MAX_LEVEL) return MAX_LEVEL;
    return (uint8_t)level;
}

uint8_t BrightnessModel::levelToFastLED(uint8_t level) {
    return LEVEL_TABLE[clampLevel((int)level) - 1];
}

uint32_t BrightnessModel::worstCaseMilliamps(uint8_t level) {
    const uint32_t scale = levelToFastLED(level);
    return ((uint32_t)NUM_LEDS * MA_PER_LED_WHITE * scale) / 255u;
}

void BrightnessModel::renderScreen(uint8_t* buf, uint8_t level) {
    const uint8_t lv = clampLevel((int)level);

    Draw::clear(buf);

    // ── Level number, centred ─────────────────────────────────────────────────
    char num[4];
    snprintf(num, sizeof(num), "%u", (unsigned)lv);

    // Warm ramp: amber at the low end, near-white at the top.
    const uint8_t warm[3] = {
        255,
        (uint8_t)(140 + (uint16_t)(115 * (lv - 1)) / (MAX_LEVEL - 1)),
        (uint8_t)( 40 + (uint16_t)(180 * (lv - 1)) / (MAX_LEVEL - 1)),
    };

    // Sized in PIXELS, not characters: the buffer is textWidth * CHAR_HEIGHT * 3
    // bytes and "10" is 7px wide. 16px covers the widest level label.
    uint8_t  txt[16 * SmallTextRenderer::CHAR_HEIGHT * 3];
    uint16_t w = 0, h = 0;
    SmallTextRenderer::renderText(num, txt, w, h, warm);
    Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 3);

    // ── Proportional bar ──────────────────────────────────────────────────────
    // 14px track inset one pixel each side; fill rounds so level 10 fills it
    // completely and level 1 always shows at least one lit pixel.
    const uint8_t TRACK_X = 1, TRACK_W = 14, BAR_Y = 9, BAR_H = 3;

    Draw::rect(buf, TRACK_X, BAR_Y, TRACK_W, BAR_H, 14, 14, 16);

    uint8_t fill = (uint8_t)(((uint16_t)TRACK_W * lv + MAX_LEVEL / 2) / MAX_LEVEL);
    if (fill < 1)       fill = 1;
    if (fill > TRACK_W) fill = TRACK_W;

    Draw::rect(buf, TRACK_X, BAR_Y, fill, BAR_H, warm[0], warm[1], warm[2]);

    // End caps mark the range so the bar reads as a scale, not a progress bar.
    Draw::px(buf, TRACK_X - 1,           BAR_Y + 1, 60, 60, 70);
    Draw::px(buf, TRACK_X + TRACK_W,     BAR_Y + 1, 60, 60, 70);
}
