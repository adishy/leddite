#pragma once

#include "Canvas.h"
#include <stdint.h>

// OctopusMode — animated character display.
//
// Characters (NUM_CHARS): currently Ghost only; add more by incrementing
//   NUM_CHARS and adding a draw + style table entry.
//
// Encoder turn: cycle character (no-op when NUM_CHARS == 1)
// Short press:  cycle colour style within current character (5 palettes)
// Long press (handled by .ino): back to menu
class OctopusMode {
public:
    static const uint8_t NUM_CHARS        = 1;   // extend to add more characters
    static const uint8_t NUM_GHOST_STYLES = 5;
    static const uint8_t NUM_TENTACLES    = 4;   // ghost skirt legs

    void begin(Canvas& canvas);
    void update(Canvas& canvas);
    void onEncoderTurn(int delta);   // cycle character
    void onEncoderPress();           // cycle colour style within current character
    void nextStyle();                // alias for onEncoderPress()

private:
    // ── Ghost colour style ────────────────────────────────────────────────────
    struct GhostStyle {
        uint8_t topR, topG, topB;
        uint8_t botR, botG, botB;
        uint8_t hiR,  hiG,  hiB;
    };
    static const GhostStyle GHOST_STYLES[NUM_GHOST_STYLES];

    // ── Animation ─────────────────────────────────────────────────────────────
    enum class AnimState { IDLE, MOVING };

    static const uint32_t FRAME_MS      = 110;
    static const uint8_t  MOVE_FRAMES   = 8;
    static const uint32_t IDLE_BASE_MS  = 1800;
    static const uint32_t IDLE_RANGE_MS = 1600;
    static const uint32_t DRAW_MIN_MS   = 33;

    // Ghost animation tables
    static const int8_t  FRAME_YOFF  [MOVE_FRAMES];
    static const uint8_t FRAME_TPHASE[MOVE_FRAMES];
    static const bool    FRAME_BLINK [MOVE_FRAMES];

    // ── Ghost body shape ──────────────────────────────────────────────────────
    struct Span { uint8_t x0, x1; };
    static const Span BODY[9];
    static const uint8_t TENT[2][NUM_TENTACLES][5][2];

    // ── State ─────────────────────────────────────────────────────────────────
    uint8_t   currentChar = 0;
    uint8_t   ghostStyle  = 0;
    AnimState animState   = AnimState::IDLE;
    uint32_t  stateMs     = 0;
    uint32_t  idleDurMs   = 2000;
    uint8_t   moveFrame   = 0;
    uint32_t  frameMs     = 0;
    uint32_t  drawMs      = 0;

    uint8_t frameBuf[16 * 16 * 3];   // scratch — avoids stack allocation

    // ── Rendering ─────────────────────────────────────────────────────────────
    void resetAnim();
    void drawGhost(Canvas& canvas, int8_t yOff, uint8_t legPhase,
                   bool eyesClosed, const GhostStyle& s);
    static void px(uint8_t* buf, int x, int y, uint8_t r, uint8_t g, uint8_t b);
};
