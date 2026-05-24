#pragma once

#include "Canvas.h"
#include <stdint.h>

// OctopusMode — animated chibi octopus display.
//
// One octopus on screen at a time. Stop/go animation: rests for ~2–3 s,
// then bobs and waves all eight tentacles for ~880 ms, then rests again.
// Large chibi eyes (3×3 sclera, 2×2 pupil, 1-px highlight), no mouth.
//
// Encoder turn or short press: cycle through NUM_STYLES colour variants.
// Long press (handled by .ino): back to menu.
class OctopusMode {
public:
    static const uint8_t NUM_STYLES    = 5;
    static const uint8_t NUM_TENTACLES = 8;

    void begin(Canvas& canvas);
    void update(Canvas& canvas);
    void onEncoderTurn(int delta);  // cycle style (+1 / -1)
    void nextStyle();               // convenience: onEncoderTurn(1)

private:
    // ── Colour style ─────────────────────────────────────────────────────────
    struct Style {
        uint8_t topR, topG, topB;   // upper body
        uint8_t botR, botG, botB;   // lower body + tentacles
        uint8_t hiR,  hiG,  hiB;   // specular highlight
        uint8_t eyeR, eyeG, eyeB;  // eye sclera
        uint8_t pupR, pupG, pupB;  // pupil
    };
    static const Style STYLES[NUM_STYLES];

    // ── Animation ─────────────────────────────────────────────────────────────
    enum class AnimState { IDLE, MOVING };

    static const uint32_t FRAME_MS      = 110;   // ms per animation frame
    static const uint8_t  MOVE_FRAMES   = 8;
    static const uint32_t IDLE_BASE_MS  = 1800;
    static const uint32_t IDLE_RANGE_MS = 1600;
    static const uint32_t DRAW_MIN_MS   = 33;    // ~30 FPS cap

    static const int8_t  FRAME_YOFF  [MOVE_FRAMES];
    static const uint8_t FRAME_TPHASE[MOVE_FRAMES];
    static const bool    FRAME_BLINK [MOVE_FRAMES];

    // ── Body shape ────────────────────────────────────────────────────────────
    // Nine horizontal spans for body rows y=1..9 (yOff=0)
    struct Span { uint8_t x0, x1; };
    static const Span BODY[9];

    // Eight tentacles, two phases, five points each: [phase][tentacle][point][x,y]
    static const uint8_t TENT[2][NUM_TENTACLES][5][2];

    // ── State ─────────────────────────────────────────────────────────────────
    uint8_t   styleIdx  = 0;
    AnimState animState = AnimState::IDLE;
    uint32_t  stateMs   = 0;
    uint32_t  idleDurMs = 2000;
    uint8_t   moveFrame = 0;
    uint32_t  frameMs   = 0;
    uint32_t  drawMs    = 0;

    uint8_t   frameBuf[16 * 16 * 3];   // scratch — avoids stack allocation

    // ── Rendering ─────────────────────────────────────────────────────────────
    void drawFrame(Canvas& canvas, int8_t yOff, uint8_t tentPhase,
                   bool eyesClosed, const Style& s);
    static void px(uint8_t* buf, int x, int y, uint8_t r, uint8_t g, uint8_t b);
};
