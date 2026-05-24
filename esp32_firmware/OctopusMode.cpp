#include "OctopusMode.h"
#include <Arduino.h>
#include <string.h>

// ── Ghost colour palettes ─────────────────────────────────────────────────────
//                            top body          bot body          highlight         pupils
const OctopusMode::GhostStyle OctopusMode::GHOST_STYLES[OctopusMode::NUM_GHOST_STYLES] = {
    { 230, 55, 45,  185, 25, 15,  255,155,150,   30, 90,230 },  // 0: Blinky  (red,    blue pupils)
    { 245,105,200,  200, 60,165,  255,210,240,   30, 90,230 },  // 1: Pinky   (pink,   blue pupils)
    {  30,210,225,   15,155,190,  165,245,255,   30, 90,230 },  // 2: Inky    (cyan,   blue pupils)
    { 235,155, 35,  190,110, 10,  255,225,155,   30, 90,230 },  // 3: Clyde   (orange, blue pupils)
    {  45, 65,240,   20, 35,195,  130,155,255,  255,240,180 },  // 4: Scared  (blue,   pale pupils)
};

// ── Nyan Cat colour palettes ──────────────────────────────────────────────────
//                           cat body       pop-tart base   frosting
const OctopusMode::NyanStyle OctopusMode::NYAN_STYLES[OctopusMode::NUM_NYAN_STYLES] = {
    { 180,180,180,  210,175,130,  255,130,170 },  // 0: Classic   (gray, tan, pink)
    {  20, 30, 80,   10, 10, 10,   50,230,220 },  // 1: Space     (dark blue, black, cyan)
    { 255,240,245,  255,240,245,  255,160,200 },  // 2: Sakura    (white, white, rose)
    {  15, 15, 15,  210,100, 20,  160, 30,200 },  // 3: Halloween (black, orange, purple)
    {  50,200,190,   30, 40, 50,   80,220,210 },  // 4: Miku      (teal, dark, teal)
};

// ── Animation tables ──────────────────────────────────────────────────────────
// Frames:  0    1    2    3    4    5    6    7
const int8_t  OctopusMode::FRAME_YOFF  [OctopusMode::MOVE_FRAMES] = { 0,  0,  1,  1,  0,  0,  1,  0 };
const uint8_t OctopusMode::FRAME_TPHASE[OctopusMode::MOVE_FRAMES] = { 0,  1,  1,  0,  0,  1,  1,  0 };
const bool    OctopusMode::FRAME_BLINK [OctopusMode::MOVE_FRAMES] = { 0,  0,  0,  0,  0,  0,  1,  0 };

const uint8_t OctopusMode::NYAN_LEGS[OctopusMode::MOVE_FRAMES] = { 0, 0, 1, 1, 0, 0, 1, 1 };
const bool    OctopusMode::NYAN_STAR[OctopusMode::MOVE_FRAMES] = { 1, 0, 0, 1, 1, 0, 0, 1 };

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
    nyanStyle   = 0;
    resetAnim();
    drawGhost(canvas, 0, 0, false, GHOST_STYLES[ghostStyle]);
}

void OctopusMode::onEncoderTurn(int delta) {
    currentChar = (uint8_t)((currentChar + NUM_CHARS + delta) % NUM_CHARS);
    resetAnim();
}

void OctopusMode::onEncoderPress() {
    if (currentChar == 0)
        ghostStyle = (uint8_t)((ghostStyle + 1) % NUM_GHOST_STYLES);
    else
        nyanStyle  = (uint8_t)((nyanStyle  + 1) % NUM_NYAN_STYLES);
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
        if (currentChar == 0)
            drawGhost(canvas, 0, 0, blink, GHOST_STYLES[ghostStyle]);
        else
            drawNyanCat(canvas, NYAN_LEGS[0], NYAN_STAR[0] && !blink, NYAN_STYLES[nyanStyle]);

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
                if (currentChar == 0)
                    drawGhost(canvas, 0, 0, false, GHOST_STYLES[ghostStyle]);
                else
                    drawNyanCat(canvas, NYAN_LEGS[0], true, NYAN_STYLES[nyanStyle]);
                return;
            }
        }
        uint8_t f = moveFrame;
        if (currentChar == 0)
            drawGhost(canvas, FRAME_YOFF[f], FRAME_TPHASE[f], FRAME_BLINK[f], GHOST_STYLES[ghostStyle]);
        else
            drawNyanCat(canvas, NYAN_LEGS[f], NYAN_STAR[f], NYAN_STYLES[nyanStyle]);
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
            px(frameBuf, 4,y,s.pupR,s.pupG,s.pupB); px(frameBuf, 5,y,s.pupR,s.pupG,s.pupB);
            px(frameBuf,10,y,s.pupR,s.pupG,s.pupB); px(frameBuf,11,y,s.pupR,s.pupG,s.pupB);
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

// ── Nyan Cat drawing ──────────────────────────────────────────────────────────
//
// 16×16 layout (cat faces right):
//
//   Rainbow stripes (x=0..6, 2 rows each):
//     y= 3-4  red      (255, 30, 30)
//     y= 5-6  orange   (255,150,  0)
//     y= 7-8  yellow   (255,230,  0)
//     y= 9-10 green    ( 50,220, 50)
//     y=11-12 blue     ( 50,100,255)
//     y=13-14 violet   (170, 50,255)
//
//   Pop-tart:  x=7..13, y=3..12 (style.pastry)
//   Frosting:  x=8..12, y=4..11 (style.frost)
//   4 sprinkles inside frosting (hardcoded)
//   Cat body:  x=10..14, y=4..10 (style.cat), overlaid on poptart
//   Cat head:  x=12..15, y=1..6  (style.cat)
//   Ears:      x=12,y=0 and x=15,y=0 (half-dark)
//   Eyes:      white at (13,3)+(15,3), black pupil at (13,4)+(15,4)
//   Nose:      pink at (14,5)
//   Whisker hints: white at (13,6) and (15,6)
//   Legs (4, x=8,10,12,14): alternating phase 0/1
//   Stars:     pale yellow at (2,0) and (5,14) when starOn

void OctopusMode::drawNyanCat(Canvas& canvas, uint8_t legPhase, bool starOn,
                                const NyanStyle& s) {
    memset(frameBuf, 0, sizeof(frameBuf));

    // Rainbow stripes
    static const uint8_t RB[6][3] = {
        {255, 30, 30},
        {255,150,  0},
        {255,230,  0},
        { 50,220, 50},
        { 50,100,255},
        {170, 50,255},
    };
    for (uint8_t c = 0; c < 6; c++) {
        int y0 = 3 + c * 2;
        for (int y = y0; y <= y0 + 1; y++)
            for (int x = 0; x <= 6; x++)
                px(frameBuf, x, y, RB[c][0], RB[c][1], RB[c][2]);
    }

    // Pop-tart body
    for (int y = 3; y <= 12; y++)
        for (int x = 7; x <= 13; x++)
            px(frameBuf, x, y, s.pastR, s.pastG, s.pastB);

    // Frosting
    for (int y = 4; y <= 11; y++)
        for (int x = 8; x <= 12; x++)
            px(frameBuf, x, y, s.frostR, s.frostG, s.frostB);

    // Sprinkles
    px(frameBuf,  9,  5, 200, 50, 50);
    px(frameBuf, 11,  6,  50,150, 50);
    px(frameBuf,  8,  9, 100, 50,200);
    px(frameBuf, 12, 10,  50,150,200);

    // Cat body (overlaid on poptart)
    for (int y = 4; y <= 10; y++)
        for (int x = 10; x <= 14; x++)
            px(frameBuf, x, y, s.catR, s.catG, s.catB);

    // Cat head
    for (int y = 1; y <= 6; y++)
        for (int x = 12; x <= 15; x++)
            px(frameBuf, x, y, s.catR, s.catG, s.catB);

    // Ear tips (half brightness)
    px(frameBuf, 12, 0, s.catR/2, s.catG/2, s.catB/2);
    px(frameBuf, 15, 0, s.catR/2, s.catG/2, s.catB/2);

    // Eyes
    px(frameBuf, 13, 3, 255,255,255);
    px(frameBuf, 15, 3, 255,255,255);
    px(frameBuf, 13, 4,   0,  0,  0);
    px(frameBuf, 15, 4,   0,  0,  0);

    // Nose
    px(frameBuf, 14, 5, 255,150,170);

    // Whisker hints
    px(frameBuf, 13, 6, 255,255,255);
    px(frameBuf, 15, 6, 255,255,255);

    // Legs (2px tall, alternating phase)
    int lyFront = (legPhase == 0) ? 13 : 12;
    int lyBack  = (legPhase == 0) ? 12 : 13;
    // Front legs (x=8,10) follow lyFront
    for (int dy = 0; dy <= 1; dy++) {
        px(frameBuf,  8, lyFront - dy, s.catR, s.catG, s.catB);
        px(frameBuf, 10, lyFront - dy, s.catR, s.catG, s.catB);
    }
    // Back legs (x=12,14) follow lyBack
    for (int dy = 0; dy <= 1; dy++) {
        px(frameBuf, 12, lyBack - dy, s.catR, s.catG, s.catB);
        px(frameBuf, 14, lyBack - dy, s.catR, s.catG, s.catB);
    }

    // Stars
    if (starOn) {
        px(frameBuf,  2,  0, 255,255,200);
        px(frameBuf,  5, 14, 255,255,200);
    }

    canvas.drawSprite(frameBuf, 16, 16, 0, 0, 0, true);
}
