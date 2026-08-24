#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

// GameEngine — the auto-playing game screensavers.
//
// Pure C++/stdint only: no Arduino, no FastLED, no millis(), no random().
// Time is injected via update(nowMs) and randomness comes from a seeded
// xorshift32, so a given (seed, time sequence) always produces the same frames.
// That determinism is what lets the unit tests assert on real pixel output.
//
// Each game is written to *never end*: there are no game-over screens and no
// stalls. Losing states soft-reset into a fresh round, because this is a
// screensaver — it has to look alive indefinitely with nobody watching.
//
//   SNAKE     greedy-with-noise AI; walls bump instead of killing
//   LIFE      Conway on a 32x32 torus, camera pans to the liveliest window
//   INVADERS  marching formation, auto-aiming cannon, endless waves
//   DINO      endless runner, auto-jumps obstacles, day/night cycle
//
// Render target is a 16*16*3 buffer (see Draw.h).

#include <stdint.h>

enum class Game : uint8_t {
    SNAKE    = 0,
    LIFE     = 1,
    INVADERS = 2,
    DINO     = 3,
    COUNT    = 4,
};

class GameEngine {
public:
    static const uint8_t LIFE_WORLD  = 32;   // toroidal world edge (viewport is 16)
    static const uint8_t SNAKE_MAX   = 96;   // body cap; longer than this looks solid

    // Per-game step intervals in ms — each game's natural pace.
    static const uint16_t SNAKE_STEP_MS    = 130;
    static const uint16_t LIFE_STEP_MS     = 150;
    static const uint16_t INVADERS_STEP_MS = 110;
    static const uint16_t DINO_STEP_MS     = 60;

    void begin(Game g, uint32_t nowMs, uint32_t seed);
    void update(uint32_t nowMs);           // advances at the game's own rate
    void reset(uint32_t nowMs);            // restart the current game

    const uint8_t* buffer() const { return buf; }
    Game           game()   const { return cur; }

    // Exposed for unit tests — cheap invariants a test can assert against
    // without having to decode pixels.
    uint16_t snakeLength()   const { return snLen; }
    uint16_t lifePopulation() const;
    uint8_t  invadersAlive() const;
    bool     dinoGrounded()  const { return dinoY == 0 && dinoVY == 0; }
    uint32_t stepCount()     const { return steps; }

private:
    // ── Deterministic PRNG ────────────────────────────────────────────────────
    uint32_t rnd32();
    uint8_t  rndN(uint8_t n);              // uniform-ish in [0, n)

    // ── Per-game lifecycle ────────────────────────────────────────────────────
    void initSnake();
    void stepSnake();
    void drawSnake();

    void initLife();
    void stepLife();
    void drawLife();
    uint16_t windowActivity(uint8_t x0, uint8_t y0) const;

    void initInvaders();
    void stepInvaders();
    void drawInvaders();
    void invaderPos(uint8_t i, int8_t& x, int8_t& y) const;

    void initDino();
    void stepDino();
    void drawDino();

    static uint8_t wrapWorld(int16_t v);

    // ── Shared state ──────────────────────────────────────────────────────────
    uint8_t  buf[16 * 16 * 3];
    uint32_t rngState   = 1;
    Game     cur        = Game::SNAKE;
    uint32_t lastStepMs = 0;
    uint32_t steps      = 0;

    // ── Snake ─────────────────────────────────────────────────────────────────
    uint8_t  snX[SNAKE_MAX], snY[SNAKE_MAX];
    uint16_t snLen = 0;
    int8_t   snDX = 1, snDY = 0;
    uint8_t  foodX = 0, foodY = 0;

    // ── Life ──────────────────────────────────────────────────────────────────
    uint8_t  lAlive[LIFE_WORLD * LIFE_WORLD];
    uint8_t  lAge  [LIFE_WORLD * LIFE_WORLD];
    uint8_t  lAct  [LIFE_WORLD * LIFE_WORLD];   // decayed change heatmap
    uint8_t  lNext [LIFE_WORLD * LIFE_WORLD];
    uint8_t  lNextAge[LIFE_WORLD * LIFE_WORLD];
    uint16_t lGen  = 0;
    int16_t  camX = 0, camY = 0;                 // viewport top-left, /16 fixed point
    int16_t  camTX = 0, camTY = 0;

    // ── Invaders ──────────────────────────────────────────────────────────────
    // 4 columns at 3px pitch spans 10px, leaving 6px of travel in a 16px field.
    // A 5th column left only 3px, so the formation bounced every ~3 steps and
    // rained to the bottom before the cannon could land a single shot.
    static const uint8_t INV_COLS    = 4;
    static const uint8_t INV_ROWS    = 3;
    static const uint8_t INV_MOVE_EVERY = 3;     // formation steps at 1/3 bullet rate
    uint16_t invMask = 0;                        // bit i set = invader i alive
    int8_t   invX = 1, invY = 1, invDir = 1;
    uint8_t  invFrame = 0;                       // wiggle animation phase
    uint8_t  invTick  = 0;                       // counts up to INV_MOVE_EVERY
    int8_t   bulX = -1, bulY = -1;               // player bullet (-1 = none)
    int8_t   canX = 7;                           // cannon centre
    int8_t   expX = -1, expY = -1;
    uint8_t  expTtl = 0;
    uint8_t  wave = 0;

    // ── Dino ──────────────────────────────────────────────────────────────────
    static const uint8_t  OBS_MAX    = 3;
    static const int16_t  JUMP_V0    = 22;       // peak ~5.2px — clears a 4px cactus
    static const int16_t  JUMP_GRAV  = 6;        // and stays on a 16px screen
    int16_t  dinoY  = 0;                         // height above ground, /10 px
    int16_t  dinoVY = 0;                         // velocity, /10 px per step
    int16_t  obsX[OBS_MAX];                      // /10 px; <0 means the slot is free
    uint8_t  obsH[OBS_MAX];
    uint16_t dinoSpeed = 10;                     // /10 px per step
    uint32_t dinoDist  = 0;
    bool     night     = false;
};

#endif // GAME_ENGINE_H
