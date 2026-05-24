#include "OctopusMode.h"
#include <Arduino.h>
#include <string.h>

// ── Colour palettes ───────────────────────────────────────────────────────────
//                         top body          bot body          highlight         sclera            pupil
const OctopusMode::Style OctopusMode::STYLES[OctopusMode::NUM_STYLES] = {
    { 110,190,255,  60,130,220, 200,230,255, 240,245,255,  20, 50,110 },  // 0: Pale Blue Ocean
    {  60,200, 90, 130,195, 35, 190,240,150, 255,255,215,  25, 70, 15 },  // 1: Verdant Meadow
    { 170, 80,230,  90, 40,175, 210,145,255, 255,230,255,  55, 20, 95 },  // 2: Amethyst Dream
    { 245,120, 85, 200, 65, 45, 255,185,155, 255,245,230,  85, 30, 15 },  // 3: Coral Sunset
    { 245,195, 40, 200,145,  5, 255,235,130, 255,255,220,  75, 45,  0 },  // 4: Golden Star
};

// ── Animation tables ──────────────────────────────────────────────────────────
// Frames:  0    1    2    3    4    5    6    7
const int8_t  OctopusMode::FRAME_YOFF  [OctopusMode::MOVE_FRAMES] = { 0,  0,  1,  1,  0,  0,  1,  0 };
const uint8_t OctopusMode::FRAME_TPHASE[OctopusMode::MOVE_FRAMES] = { 0,  1,  1,  0,  0,  1,  1,  0 };
const bool    OctopusMode::FRAME_BLINK [OctopusMode::MOVE_FRAMES] = { 0,  0,  0,  0,  0,  0,  1,  0 };

// ── Body spans (y = 1+row+yOff, row 0..8) ────────────────────────────────────
const OctopusMode::Span OctopusMode::BODY[9] = {
    {4, 11},  // row 0 → y=1
    {3, 12},  // row 1 → y=2
    {2, 13},  // row 2 → y=3
    {2, 13},  // row 3 → y=4
    {2, 13},  // row 4 → y=5
    {2, 13},  // row 5 → y=6
    {3, 12},  // row 6 → y=7
    {4, 11},  // row 7 → y=8
    {5, 10},  // row 8 → y=9  (mantle base)
};

// ── Eight tentacles — two phases ─────────────────────────────────────────────
// [phase][tentacle 0-7][point 0-4][x, y]
// y values are absolute canvas coords at yOff=0; yOff is added at draw time.
const uint8_t OctopusMode::TENT[2][OctopusMode::NUM_TENTACLES][5][2] = {
    // ── Phase 0 (rest) ───────────────────────────────────────────────────────
    {
        {{ 1,10},{ 0,11},{ 1,12},{ 0,13},{ 1,14}},  // T0  far-left outer
        {{ 3,10},{ 3,11},{ 2,12},{ 3,13},{ 3,14}},  // T1  far-left inner
        {{ 5,10},{ 5,11},{ 4,12},{ 5,13},{ 5,14}},  // T2  left
        {{ 7,10},{ 7,11},{ 8,12},{ 7,13},{ 7,14}},  // T3  centre-left
        {{ 8,10},{ 8,11},{ 7,12},{ 8,13},{ 8,14}},  // T4  centre-right
        {{10,10},{10,11},{11,12},{10,13},{10,14}},   // T5  right
        {{12,10},{12,11},{13,12},{12,13},{12,14}},   // T6  far-right inner
        {{14,10},{15,11},{14,12},{15,13},{14,14}},   // T7  far-right outer
    },
    // ── Phase 1 (waved) ──────────────────────────────────────────────────────
    {
        {{ 2,10},{ 1,11},{ 0,12},{ 1,13},{ 2,14}},  // T0
        {{ 3,10},{ 2,11},{ 3,12},{ 4,13},{ 3,14}},  // T1
        {{ 5,10},{ 6,11},{ 5,12},{ 4,13},{ 5,14}},  // T2
        {{ 7,10},{ 8,11},{ 9,12},{ 8,13},{ 7,14}},  // T3
        {{ 8,10},{ 7,11},{ 6,12},{ 7,13},{ 8,14}},  // T4
        {{10,10},{ 9,11},{10,12},{11,13},{10,14}},   // T5
        {{12,10},{13,11},{12,12},{11,13},{12,14}},   // T6
        {{14,10},{13,11},{14,12},{15,13},{14,14}},   // T7
    },
};

// ── Public API ────────────────────────────────────────────────────────────────

void OctopusMode::begin(Canvas& canvas) {
    animState  = AnimState::IDLE;
    stateMs    = millis();
    idleDurMs  = IDLE_BASE_MS;
    moveFrame  = 0;
    drawMs     = 0;
    drawFrame(canvas, 0, 0, false, STYLES[styleIdx]);
}

void OctopusMode::onEncoderTurn(int delta) {
    styleIdx  = (uint8_t)((styleIdx + NUM_STYLES + delta) % NUM_STYLES);
    // Snap back to rest so the new style appears immediately
    animState = AnimState::IDLE;
    stateMs   = millis();
    idleDurMs = IDLE_BASE_MS;
    moveFrame = 0;
    drawMs    = 0;
}

void OctopusMode::nextStyle() {
    onEncoderTurn(1);
}

void OctopusMode::update(Canvas& canvas) {
    uint32_t now = millis();
    if (now - drawMs < DRAW_MIN_MS) return;
    drawMs = now;

    if (animState == AnimState::IDLE) {
        // Blink for 120 ms every 4 s
        bool blink = (now % 4000) < 120;
        drawFrame(canvas, 0, 0, blink, STYLES[styleIdx]);

        if (now - stateMs >= idleDurMs) {
            animState = AnimState::MOVING;
            stateMs   = now;
            frameMs   = now;
            moveFrame = 0;
        }
    } else {
        // Advance frame
        if (now - frameMs >= FRAME_MS) {
            frameMs = now;
            moveFrame++;
            if (moveFrame >= MOVE_FRAMES) {
                animState = AnimState::IDLE;
                stateMs   = now;
                idleDurMs = IDLE_BASE_MS + (now % IDLE_RANGE_MS);
                moveFrame = 0;
                drawFrame(canvas, 0, 0, false, STYLES[styleIdx]);
                return;
            }
        }
        drawFrame(canvas,
                  FRAME_YOFF  [moveFrame],
                  FRAME_TPHASE[moveFrame],
                  FRAME_BLINK [moveFrame],
                  STYLES[styleIdx]);
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void OctopusMode::px(uint8_t* buf, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;
    int idx = (y * 16 + x) * 3;
    buf[idx]     = r;
    buf[idx + 1] = g;
    buf[idx + 2] = b;
}

void OctopusMode::drawFrame(Canvas& canvas, int8_t yOff, uint8_t tentPhase,
                              bool eyesClosed, const Style& s) {
    memset(frameBuf, 0, sizeof(frameBuf));

    // ── Body (9 rows, vertical gradient top→bottom) ───────────────────────────
    for (uint8_t row = 0; row < 9; row++) {
        int y = 1 + row + (int)yOff;
        if (y < 0 || y >= 16) continue;

        // Lerp top→bot over the lower 4 rows (rows 5-8)
        uint8_t r, g, b;
        if (row < 5) {
            r = s.topR; g = s.topG; b = s.topB;
        } else {
            uint8_t t = row - 4;  // 1..4
            r = (uint8_t)(s.topR + (int16_t)(s.botR - s.topR) * t / 4);
            g = (uint8_t)(s.topG + (int16_t)(s.botG - s.topG) * t / 4);
            b = (uint8_t)(s.topB + (int16_t)(s.botB - s.topB) * t / 4);
        }
        for (int x = BODY[row].x0; x <= BODY[row].x1; x++) {
            px(frameBuf, x, y, r, g, b);
        }
    }

    // ── Specular highlight (upper-left of dome) ───────────────────────────────
    px(frameBuf, 5, 1 + (int)yOff, s.hiR, s.hiG, s.hiB);
    px(frameBuf, 6, 1 + (int)yOff, s.hiR, s.hiG, s.hiB);
    px(frameBuf, 4, 2 + (int)yOff, s.hiR, s.hiG, s.hiB);
    px(frameBuf, 5, 2 + (int)yOff, s.hiR, s.hiG, s.hiB);

    // ── Eyes ──────────────────────────────────────────────────────────────────
    if (!eyesClosed) {
        // Left eye — 3×3 sclera at (3..5, 3+yOff..5+yOff)
        for (int ey = 3; ey <= 5; ey++) {
            int y = ey + (int)yOff;
            if (y < 0 || y >= 16) continue;
            px(frameBuf, 3, y, s.eyeR, s.eyeG, s.eyeB);
            px(frameBuf, 4, y, s.eyeR, s.eyeG, s.eyeB);
            px(frameBuf, 5, y, s.eyeR, s.eyeG, s.eyeB);
        }
        // Left pupil — 2×2 at (4..5, 4+yOff..5+yOff)
        for (int ey = 4; ey <= 5; ey++) {
            int y = ey + (int)yOff;
            if (y < 0 || y >= 16) continue;
            px(frameBuf, 4, y, s.pupR, s.pupG, s.pupB);
            px(frameBuf, 5, y, s.pupR, s.pupG, s.pupB);
        }
        px(frameBuf, 3, 3 + (int)yOff, 255, 255, 255);  // left highlight dot

        // Right eye — 3×3 sclera at (10..12, 3+yOff..5+yOff)
        for (int ey = 3; ey <= 5; ey++) {
            int y = ey + (int)yOff;
            if (y < 0 || y >= 16) continue;
            px(frameBuf, 10, y, s.eyeR, s.eyeG, s.eyeB);
            px(frameBuf, 11, y, s.eyeR, s.eyeG, s.eyeB);
            px(frameBuf, 12, y, s.eyeR, s.eyeG, s.eyeB);
        }
        // Right pupil — 2×2 at (10..11, 4+yOff..5+yOff)
        for (int ey = 4; ey <= 5; ey++) {
            int y = ey + (int)yOff;
            if (y < 0 || y >= 16) continue;
            px(frameBuf, 10, y, s.pupR, s.pupG, s.pupB);
            px(frameBuf, 11, y, s.pupR, s.pupG, s.pupB);
        }
        px(frameBuf, 12, 3 + (int)yOff, 255, 255, 255);  // right highlight dot
    } else {
        // Blink: thin horizontal squiggle where the eye was
        int y = 4 + (int)yOff;
        if (y >= 0 && y < 16) {
            px(frameBuf,  3, y, s.pupR, s.pupG, s.pupB);
            px(frameBuf,  4, y, s.pupR, s.pupG, s.pupB);
            px(frameBuf,  5, y, s.pupR, s.pupG, s.pupB);
            px(frameBuf, 10, y, s.pupR, s.pupG, s.pupB);
            px(frameBuf, 11, y, s.pupR, s.pupG, s.pupB);
            px(frameBuf, 12, y, s.pupR, s.pupG, s.pupB);
        }
    }

    // ── Eight tentacles ───────────────────────────────────────────────────────
    for (uint8_t t = 0; t < NUM_TENTACLES; t++) {
        for (uint8_t p = 0; p < 5; p++) {
            int x = TENT[tentPhase][t][p][0];
            int y = TENT[tentPhase][t][p][1] + (int)yOff;
            px(frameBuf, x, y, s.botR, s.botG, s.botB);
        }
    }

    canvas.drawSprite(frameBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}
