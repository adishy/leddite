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
//   INVADERS  endless escalating levels, enemy fire, periodic bosses
//   DINO      endless runner with a T-Rex and lookahead-planned jumps
//   PONG      two AI paddles, multicoloured trailing balls
//   BREAKOUT  AI paddle clearing multicoloured brick fields, endless levels
//
// Render target is a 16*16*3 buffer (see Draw.h).

#include <stdint.h>

enum class Game : uint8_t {
    SNAKE    = 0,
    INVADERS = 1,
    DINO     = 2,
    PONG     = 3,
    BREAKOUT = 4,
    COUNT    = 5,
};

class GameEngine {
public:
    static const uint8_t SNAKE_MAX = 96;   // body cap; longer than this looks solid

    // Per-game step intervals in ms — each game's natural pace.
    static const uint16_t SNAKE_STEP_MS    = 130;
    static const uint16_t INVADERS_STEP_MS = 110;
    static const uint16_t DINO_STEP_MS     = 60;
    static const uint16_t PONG_STEP_MS     = 55;
    static const uint16_t BREAKOUT_STEP_MS = 55;

    void begin(Game g, uint32_t nowMs, uint32_t seed);
    void update(uint32_t nowMs);           // advances at the game's own rate
    void reset(uint32_t nowMs);            // restart the current game

    const uint8_t* buffer() const { return buf; }
    Game           game()   const { return cur; }

    static uint16_t stepIntervalMs(Game g);

    // ── Test / tuning surface ────────────────────────────────────────────────
    // Cheap invariants a test can assert on without decoding pixels.
    uint16_t snakeLength()   const { return snLen; }
    uint8_t  invadersAlive() const;
    bool     dinoGrounded()  const { return dinoY == 0 && dinoVY == 0; }
    uint32_t stepCount()     const { return steps; }

    // ── Invaders campaign ────────────────────────────────────────────────────
    // The cannon is meant to be overwhelmed *eventually*, not immediately, and a
    // tunable share of runs are meant to end in a win instead. `winChancePct` is
    // rolled once per campaign (see stepInvaders) and defaults to INV_WIN_PCT.
    static const uint8_t INV_WIN_PCT     = 30;   // 0.30, as the brief asks for
    static const uint8_t INV_BOSS_EVERY  = 3;    // every 3rd level is a boss
    static const uint8_t INV_CAMPAIGN    = 6;    // clearing level 6 wins the run

    void     setInvadersWinChance(uint8_t pct) { winChancePct = pct > 100 ? 100 : pct; }
    uint8_t  invadersWinChance() const { return winChancePct; }
    uint8_t  invadersLevel()     const { return invLevel; }
    // Campaign tallies since the last begin() — begin() zeroes them, so a caller
    // measuring a win rate does not have to subtract a previous session's runs.
    uint16_t invadersRuns()      const { return invRuns; }
    uint16_t invadersWins()      const { return invWins; }
    bool     invadersBossLevel() const { return bossHp > 0; }

    // Dino: a crash is a bug, not a feature — the lookahead planner is supposed
    // to make one impossible. The counter exists so a soak test can prove it.
    uint32_t dinoCrashes() const { return dinoCrashCount; }

    uint8_t  breakoutBricksLeft() const;
    uint8_t  breakoutLevel()      const { return bkLevel; }
    uint8_t  pongRallies()        const { return pgRally; }

private:
    // ── Deterministic PRNG ────────────────────────────────────────────────────
    uint32_t rnd32();
    uint8_t  rndN(uint8_t n);              // uniform-ish in [0, n)

    // ── Per-game lifecycle ────────────────────────────────────────────────────
    void initSnake();
    void stepSnake();
    void drawSnake();

    void initInvaders();                   // fresh campaign (level 1)
    void invadersLoadLevel();              // formation/boss for the current level
    void stepInvaders();
    void drawInvaders();
    void invaderPos(uint8_t i, int8_t& x, int8_t& y) const;
    void invadersEndRun(bool won);
    bool invadersShipShouldDodge() const;

    void initDino();
    void stepDino();
    void drawDino();
    // Would the dino's body box overlap an obstacle at height `heightT` (/10 px)
    // with obstacles displaced by `shift` (/10 px) and grown by `margin` px?
    bool dinoWouldHit(int16_t heightT, int16_t shift, uint8_t margin) const;
    bool dinoArcSafe(uint8_t delay) const; // simulate the whole arc before committing
    bool dinoCrashComing() const;          // does staying grounded hit something?

    void initPong();
    void stepPong();
    void drawPong();
    void pongServe(uint8_t i, int8_t dir);
    void pongSetBallCount(uint8_t n);

    void initBreakout();
    void breakoutLoadLevel();
    void stepBreakout();
    void drawBreakout();
    void breakoutServe();

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

    // ── Invaders ──────────────────────────────────────────────────────────────
    // The formation is laid out on a 3px horizontal pitch (invaders are 2px
    // wide, so 5 columns span 14px of a 16px field) and a 2px vertical pitch.
    // Columns and rows both grow with the level, which is where "more enemies"
    // and "eventually overwhelmed" come from.
    static const uint8_t INV_MAX_COLS = 5;
    static const uint8_t INV_MAX_ROWS = 5;
    static const uint8_t INV_MAX      = INV_MAX_COLS * INV_MAX_ROWS;   // 25
    static const uint8_t INV_EBUL_MAX = 5;    // simultaneous enemy bullets
    static const uint8_t INV_XPITCH   = 3;
    static const uint8_t INV_YPITCH   = 2;

    uint32_t invMask   = 0;                   // bit i set = invader i alive
    uint8_t  invCols   = 4, invRows = 3;
    int8_t   invX = 1, invY = 0, invDir = 1;
    uint8_t  invFrame  = 0;                   // wiggle animation phase
    uint8_t  invTick   = 0;                   // counts up to invMoveEvery
    uint8_t  invMoveEvery = 4;
    uint8_t  invFirePct   = 0;                // per-step chance of enemy fire
    int8_t   bulX = -1, bulY = -1;            // player bullet (-1 = none)
    int8_t   ebulX[INV_EBUL_MAX], ebulY[INV_EBUL_MAX];
    int8_t   canX = 7;                        // cannon centre
    int8_t   expX = -1, expY = -1;
    uint8_t  expTtl = 0;
    uint8_t  invLevel = 1;
    uint16_t invRuns = 0, invWins = 0;
    uint8_t  winChancePct = INV_WIN_PCT;
    bool     heroRun  = false;                // this campaign is scripted to win
    uint8_t  doomLevel = 0;                   // level a non-hero run dies on
    uint8_t  outcomeTtl = 0;                  // win/lose flourish countdown
    bool     outcomeWon = false;
    uint8_t  waveFlash  = 0;                  // formation recycled to the top
    uint8_t  dodgeTtl   = 0;                  // frames of the dodge spark
    int8_t   dodgeX     = 0;
    // Boss
    uint8_t  bossHp = 0, bossHpMax = 0;
    int8_t   bossX = 4, bossY = 1, bossDir = 1;
    uint8_t  bossFireTick = 0;

    // ── Dino ──────────────────────────────────────────────────────────────────
    // Sprite is 7x6 with its feet on row DINO_GROUND_Y. Heights and velocities
    // are in /10 px so the arc integrates smoothly at 60ms steps.
    static const uint8_t  OBS_MAX      = 3;
    static const int16_t  JUMP_V0      = 20;   // peak 7.7px over a 15-step flight
    static const int16_t  JUMP_GRAV    = 3;
    static const uint8_t  DINO_W       = 7;
    static const uint8_t  DINO_H       = 6;
    static const uint8_t  DINO_X       = 0;    // left edge of the sprite
    static const uint8_t  DINO_GROUND_Y = 13;  // row the feet stand on
    static const uint8_t  DINO_FLIGHT_STEPS = 15;
    int16_t  dinoY  = 0;                       // height above ground, /10 px
    int16_t  dinoVY = 0;                       // velocity, /10 px per step
    int16_t  obsX[OBS_MAX];                    // /10 px; <0 means the slot is free
    uint8_t  obsH[OBS_MAX], obsW[OBS_MAX];
    uint16_t dinoSpeed = 10;                   // /10 px per step
    uint32_t dinoDist  = 0;
    uint32_t dinoCrashCount = 0;
    uint8_t  dinoStumble = 0;                  // frames of the crash flourish
    bool     night     = false;

    // ── Pong ──────────────────────────────────────────────────────────────────
    // Positions are /16 px so the ball can carry shallow angles without the
    // integer rounding that makes a 16px field look like it only has 45s.
    static const uint8_t PONG_BALLS   = 3;
    static const uint8_t PONG_TRAIL   = 4;
    static const uint8_t PONG_PAD_H   = 4;
    static const int16_t PONG_SUB     = 16;    // fixed-point scale
    int16_t  pbX[PONG_BALLS], pbY[PONG_BALLS];
    int16_t  pbVX[PONG_BALLS], pbVY[PONG_BALLS];
    uint8_t  pbHue[PONG_BALLS], pbSpin[PONG_BALLS];
    int8_t   pbTrX[PONG_BALLS][PONG_TRAIL], pbTrY[PONG_BALLS][PONG_TRAIL];
    uint8_t  pbLive = 1;
    int16_t  padLY = 6 * PONG_SUB, padRY = 6 * PONG_SUB;   // paddle top, /16 px
    uint8_t  padLHue = 150, padRHue = 20;
    int8_t   padLErr = 0, padRErr = 0;
    uint8_t  pgScoreL = 0, pgScoreR = 0;
    uint8_t  pgRally  = 0;
    uint8_t  pgFlash  = 0;
    uint8_t  pgFlashHue = 0;

    // ── Breakout ──────────────────────────────────────────────────────────────
    static const uint8_t BK_COLS   = 8;        // 2px-wide bricks across 16px
    static const uint8_t BK_ROWS   = 5;
    static const uint8_t BK_TOP    = 1;        // first brick row
    static const uint8_t BK_TRAIL  = 4;
    static const uint8_t BK_PAD_W  = 4;
    static const int16_t BK_SUB    = 16;
    uint8_t  bkBrick[BK_ROWS][BK_COLS];        // 0 = gone, else hit points
    uint8_t  bkHue = 0;
    int16_t  bkX = 8 * BK_SUB, bkY = 10 * BK_SUB;
    int16_t  bkVX = 0, bkVY = 0;
    uint8_t  bkBallHue = 0;
    int8_t   bkTrX[BK_TRAIL], bkTrY[BK_TRAIL];
    int16_t  bkPadX = 6 * BK_SUB;
    int8_t   bkPadErr = 0;
    uint8_t  bkLevel = 1;
    uint8_t  bkMisses = 0;
    uint8_t  bkFlash = 0;
    uint8_t  bkServeDelay = 0;
    // Steps since the last brick fell, and how many re-serves that has forced.
    // See the anti-stall note in stepBreakout().
    static const uint16_t BK_IDLE_LIMIT = 300;   // ~16s at BREAKOUT_STEP_MS
    uint16_t bkIdle = 0;
    uint8_t  bkIdleServes = 0;
};

#endif // GAME_ENGINE_H
