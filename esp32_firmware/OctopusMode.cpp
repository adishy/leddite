#include "OctopusMode.h"
#include <Arduino.h>
#include <string.h>

// ── Ghost colour palettes ─────────────────────────────────────────────────────
//                            top body          bot body          highlight
//   pupils are always black (0,0,0)
const OctopusMode::GhostStyle OctopusMode::GHOST_STYLES[OctopusMode::NUM_GHOST_STYLES] = {
    { 230, 55, 45,  185, 25, 15,  255,155,150 },  // 0: Blinky  (red)
    { 245,105,200,  200, 60,165,  255,210,240 },  // 1: Pinky   (pink)
    {  30,210,225,   15,155,190,  165,245,255 },  // 2: Inky    (cyan)
    { 235,155, 35,  190,110, 10,  255,225,155 },  // 3: Clyde   (orange)
    {  45, 65,240,   20, 35,195,  130,155,255 },  // 4: Scared  (blue)
};

// ── Animation tables ──────────────────────────────────────────────────────────
// Frames:  0    1    2    3    4    5    6    7
const int8_t  OctopusMode::FRAME_YOFF  [OctopusMode::MOVE_FRAMES] = { 0,  0,  1,  1,  0,  0,  1,  0 };
const uint8_t OctopusMode::FRAME_TPHASE[OctopusMode::MOVE_FRAMES] = { 0,  1,  1,  0,  0,  1,  1,  0 };
const bool    OctopusMode::FRAME_BLINK [OctopusMode::MOVE_FRAMES] = { 0,  0,  0,  0,  0,  0,  1,  0 };

// ── Ghost body spans (y = 1+row+yOff, row 0..8) ──────────────────────────────
const OctopusMode::Span OctopusMode::BODY[9] = {
    {4, 11},  // row 0 → y=1  (dome top)
    {3, 12},  // row 1 → y=2
    {2, 13},  // row 2..8 — flat ghost body
    {2, 13},
    {2, 13},
    {2, 13},
    {2, 13},
    {2, 13},
    {2, 13},  // row 8 → y=9  (flat bottom)
};

// ── Ghost skirt legs ──────────────────────────────────────────────────────────
const uint8_t OctopusMode::TENT[2][OctopusMode::NUM_TENTACLES][5][2] = {
    {
        {{ 3,10},{ 2,11},{ 2,12},{ 1,13},{ 1,14}},  // L0 far-left,  angles out
        {{ 6,10},{ 6,11},{ 6,12},{ 5,13},{ 5,14}},  // L1 inner-left
        {{ 9,10},{ 9,11},{ 9,12},{10,13},{10,14}},   // L2 inner-right
        {{12,10},{13,11},{13,12},{14,13},{14,14}},   // L3 far-right, angles out
    },
    {   // phase 1 identical — bob via FRAME_YOFF drives animation
        {{ 3,10},{ 2,11},{ 2,12},{ 1,13},{ 1,14}},
        {{ 6,10},{ 6,11},{ 6,12},{ 5,13},{ 5,14}},
        {{ 9,10},{ 9,11},{ 9,12},{10,13},{10,14}},
        {{12,10},{13,11},{13,12},{14,13},{14,14}},
    },
};

// ── Public API ────────────────────────────────────────────────────────────────

void OctopusMode::begin(Canvas& canvas) {
    currentChar = 0;
    ghostStyle  = 0;
    resetAnim();
    drawGhost(canvas, 0, 0, false, GHOST_STYLES[ghostStyle]);
}

void OctopusMode::onEncoderTurn(int delta) {
    currentChar = (uint8_t)((currentChar + NUM_CHARS + delta) % NUM_CHARS);
    resetAnim();
}

void OctopusMode::onEncoderPress() {
    // Dispatch to per-character style cycle
    if (currentChar == 0)
        ghostStyle = (uint8_t)((ghostStyle + 1) % NUM_GHOST_STYLES);
    // Add: else if (currentChar == 1) <new character style cycle>
    resetAnim();
}

void OctopusMode::nextStyle() {
    onEncoderPress();
}

void OctopusMode::update(Canvas& canvas) {
    uint32_t now = millis();
    if (now - drawMs < DRAW_MIN_MS) return;
    drawMs = now;

    if (animState == AnimState::IDLE) {
        bool blink = (now % 4000) < 120;
        drawGhost(canvas, 0, 0, blink, GHOST_STYLES[ghostStyle]);
        // Add: else if (currentChar == 1) draw<NewChar>(...)

        if (now - stateMs >= idleDurMs) {
            animState = AnimState::MOVING;
            stateMs   = now;
            frameMs   = now;
            moveFrame = 0;
        }
    } else {
        if (now - frameMs >= FRAME_MS) {
            frameMs = now;
            moveFrame++;
            if (moveFrame >= MOVE_FRAMES) {
                animState = AnimState::IDLE;
                stateMs   = now;
                idleDurMs = IDLE_BASE_MS + (now % IDLE_RANGE_MS);
                moveFrame = 0;
                drawGhost(canvas, 0, 0, false, GHOST_STYLES[ghostStyle]);
                // Add: else if (currentChar == 1) draw<NewChar>(...)
                return;
            }
        }
        uint8_t f = moveFrame;
        drawGhost(canvas, FRAME_YOFF[f], FRAME_TPHASE[f], FRAME_BLINK[f], GHOST_STYLES[ghostStyle]);
        // Add: else if (currentChar == 1) draw<NewChar>(...)
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void OctopusMode::resetAnim() {
    animState = AnimState::IDLE;
    stateMs   = millis();
    idleDurMs = IDLE_BASE_MS;
    moveFrame = 0;
    drawMs    = 0;
}

void OctopusMode::px(uint8_t* buf, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;
    int idx = (y * 16 + x) * 3;
    buf[idx]     = r;
    buf[idx + 1] = g;
    buf[idx + 2] = b;
}

// ── Ghost drawing ─────────────────────────────────────────────────────────────

void OctopusMode::drawGhost(Canvas& canvas, int8_t yOff, uint8_t legPhase,
                              bool eyesClosed, const GhostStyle& s) {
    memset(frameBuf, 0, sizeof(frameBuf));

    // Body — 9 rows, vertical gradient top→bot
    for (uint8_t row = 0; row < 9; row++) {
        int y = 1 + row + (int)yOff;
        if (y < 0 || y >= 16) continue;
        uint8_t r, g, b;
        if (row < 5) {
            r = s.topR; g = s.topG; b = s.topB;
        } else {
            uint8_t t = row - 4;
            r = (uint8_t)(s.topR + (int16_t)(s.botR - s.topR) * t / 4);
            g = (uint8_t)(s.topG + (int16_t)(s.botG - s.topG) * t / 4);
            b = (uint8_t)(s.topB + (int16_t)(s.botB - s.topB) * t / 4);
        }
        for (int x = BODY[row].x0; x <= BODY[row].x1; x++)
            px(frameBuf, x, y, r, g, b);
    }

    // Specular highlight
    px(frameBuf, 5, 1+(int)yOff, s.hiR, s.hiG, s.hiB);
    px(frameBuf, 6, 1+(int)yOff, s.hiR, s.hiG, s.hiB);
    px(frameBuf, 4, 2+(int)yOff, s.hiR, s.hiG, s.hiB);
    px(frameBuf, 5, 2+(int)yOff, s.hiR, s.hiG, s.hiB);

    // Eyes
    if (!eyesClosed) {
        for (int ey = 3; ey <= 5; ey++) {
            int y = ey + (int)yOff;
            if (y < 0 || y >= 16) continue;
            px(frameBuf, 3,y,255,255,255); px(frameBuf, 4,y,255,255,255); px(frameBuf, 5,y,255,255,255);
            px(frameBuf,10,y,255,255,255); px(frameBuf,11,y,255,255,255); px(frameBuf,12,y,255,255,255);
        }
        for (int ey = 3; ey <= 4; ey++) {
            int y = ey + (int)yOff;
            if (y < 0 || y >= 16) continue;
            px(frameBuf, 4,y,0,0,0); px(frameBuf, 5,y,0,0,0);
            px(frameBuf,10,y,0,0,0); px(frameBuf,11,y,0,0,0);
        }
    } else {
        int y = 4 + (int)yOff;
        if (y >= 0 && y < 16) {
            px(frameBuf, 3,y,255,255,255); px(frameBuf, 4,y,255,255,255); px(frameBuf, 5,y,255,255,255);
            px(frameBuf,10,y,255,255,255); px(frameBuf,11,y,255,255,255); px(frameBuf,12,y,255,255,255);
        }
    }

    // Skirt legs
    for (uint8_t t = 0; t < NUM_TENTACLES; t++)
        for (uint8_t p = 0; p < 5; p++) {
            int x = TENT[legPhase][t][p][0];
            int y = TENT[legPhase][t][p][1] + (int)yOff;
            px(frameBuf, x, y, s.botR, s.botG, s.botB);
        }

    canvas.drawSprite(frameBuf, 16, 16, 0, 0, 0, true);
}

