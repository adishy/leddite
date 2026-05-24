#pragma once

#include "Canvas.h"
#include <stdint.h>

// OctopusMode — multi-character animated display.
//
// Characters (encoder turn cycles):
//   0: Ghost   — Pac-Man style ghost, 5 colour palettes
//   1: Nyan Cat — rainbow + pop-tart cat, 5 colour palettes
//
// Encoder turn: cycle character (Ghost ↔ Nyan Cat)
// Short press:  cycle colour style within the current character
// Long press (handled by .ino): back to menu
class OctopusMode {
public:
    static const uint8_t NUM_CHARS        = 2;
    static const uint8_t NUM_GHOST_STYLES = 5;
    static const uint8_t NUM_NYAN_STYLES  = 5;
    static const uint8_t NUM_TENTACLES    = 4;   // ghost skirt legs

    void begin(Canvas& canvas);
    void update(Canvas& canvas);
    void onEncoderTurn(int delta);   // cycle character
    void onEncoderPress();           // cycle style within current character
    void nextStyle();                // alias for onEncoderPress()

private:
    // ── Ghost colour style ────────────────────────────────────────────────────
    struct GhostStyle {
        uint8_t topR, topG, topB;
        uint8_t botR, botG, botB;
        uint8_t hiR,  hiG,  hiB;
        uint8_t pupR, pupG, pupB;
    };
    static const GhostStyle GHOST_STYLES[NUM_GHOST_STYLES];

    // ── Nyan Cat colour style ─────────────────────────────────────────────────
    struct NyanStyle {
        uint8_t catR,  catG,  catB;    // cat body colour
        uint8_t pastR, pastG, pastB;   // pop-tart base colour
        uint8_t frostR,frostG,frostB;  // frosting colour
        // Rainbow stripe colours are always the fixed 6-colour set
    };
    static const NyanStyle NYAN_STYLES[NUM_NYAN_STYLES];

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

    // Nyan Cat animation tables (same MOVE_FRAMES length)
    static const uint8_t NYAN_LEGS[MOVE_FRAMES];
    static const bool    NYAN_STAR[MOVE_FRAMES];

    // ── Ghost body shape ──────────────────────────────────────────────────────
    struct Span { uint8_t x0, x1; };
    static const Span BODY[9];
    static const uint8_t TENT[2][NUM_TENTACLES][5][2];

    // ── State ─────────────────────────────────────────────────────────────────
    uint8_t   currentChar = 0;
    uint8_t   ghostStyle  = 0;
    uint8_t   nyanStyle   = 0;
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
    void drawNyanCat(Canvas& canvas, uint8_t legPhase, bool starOn,
                     const NyanStyle& s);
    static void px(uint8_t* buf, int x, int y, uint8_t r, uint8_t g, uint8_t b);
};
