#include "MenuMode.h"
#include "TextRenderer.h"

// ── Static data ───────────────────────────────────────────────────────────────

const char* const MenuMode::LABELS[NUM_OPTIONS] = {
    "CK",   // Clock + Calendar
    "NT",   // Network Canvas
    "PT",   // Pattern Slideshow
    "TM",   // Visual Timer
};

const AppMode MenuMode::MODES[NUM_OPTIONS] = {
    AppMode::CLOCK_CAL,
    AppMode::NETWORK,
    AppMode::PATTERN,
    AppMode::TIMER,
};

// Accent color per mode label
const uint8_t MenuMode::COLORS[NUM_OPTIONS][3] = {
    {100, 200, 255},  // CK: cyan-blue
    {100, 255, 100},  // NT: green
    {255, 100, 200},  // PT: magenta
    {255, 200,  80},  // TM: amber
};

// 4 dots spaced evenly: x=3,6,9,12 across 16px canvas
const uint8_t MenuMode::DOT_X[NUM_OPTIONS] = {3, 6, 9, 12};

// ── Public API ────────────────────────────────────────────────────────────────

void MenuMode::begin(Canvas& canvas) {
    currentOption = 0;
    render(canvas);
}

void MenuMode::onEncoderTurn(int delta, Canvas& canvas) {
    currentOption = (uint8_t)((currentOption + NUM_OPTIONS + delta) % NUM_OPTIONS);
    render(canvas);
}

AppMode MenuMode::onEncoderPress(Canvas& canvas) {
    (void)canvas; // New mode's begin() handles the canvas transition
    return MODES[currentOption];
}

// ── Render ────────────────────────────────────────────────────────────────────

void MenuMode::render(Canvas& canvas) {
    canvas.clear();

    // ── Mode label ────────────────────────────────────────────────────────────
    // 2-char label = 12px wide, 7px tall.
    // Center horizontally: x = (16 - 12) / 2 = 2
    // Position in upper area: y = 3  (leaves bottom 3 rows for dots)
    const char*    label = LABELS[currentOption];
    const uint8_t* col   = COLORS[currentOption];

    uint8_t  textBuf[12 * 7 * 3] = {0};
    uint16_t w = 0, h = 0;
    TextRenderer::renderText(label, textBuf, w, h, col);

    int8_t xOff = (int8_t)((16 - (int16_t)w) / 2);  // center: (16-12)/2 = 2
    canvas.drawSprite(textBuf, (uint8_t)w, (uint8_t)h, xOff, 3, 0, /*clearBefore=*/false);

    // ── Indicator dots ────────────────────────────────────────────────────────
    for (uint8_t i = 0; i < NUM_OPTIONS; i++) {
        uint8_t dot[3];
        if (i == currentOption) {
            dot[0] = 255; dot[1] = 255; dot[2] = 255;  // selected: bright white
        } else {
            dot[0] = 30;  dot[1] = 30;  dot[2] = 30;   // unselected: dim grey
        }
        canvas.drawSprite(dot, 1, 1, (int8_t)DOT_X[i], (int8_t)DOT_Y, 0, false);
    }
}
