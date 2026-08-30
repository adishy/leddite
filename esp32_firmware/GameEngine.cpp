#include "GameEngine.h"
#include "Draw.h"
#include "ColorUtils.h"
#include <string.h>

// ── PRNG ──────────────────────────────────────────────────────────────────────
// xorshift32: tiny, fast, and reproducible across native/WASM/ESP32, which
// Arduino's random() is not.

uint32_t GameEngine::rnd32() {
    uint32_t x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x;
    return x;
}

uint8_t GameEngine::rndN(uint8_t n) {
    return n ? (uint8_t)(rnd32() % n) : 0;
}

// Truncating division rounds toward zero, which makes a sprite at -0.5px snap
// back to column 0 and linger there. Games here scroll things off the left edge,
// so they need a floor.
static int16_t floorDiv10(int16_t v) {
    return (v >= 0) ? (int16_t)(v / 10) : (int16_t)(-(((-v) + 9) / 10));
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

uint16_t GameEngine::stepIntervalMs(Game g) {
    switch (g) {
        case Game::SNAKE:    return SNAKE_STEP_MS;
        case Game::INVADERS: return INVADERS_STEP_MS;
        case Game::DINO:     return DINO_STEP_MS;
        case Game::PONG:     return PONG_STEP_MS;
        case Game::BREAKOUT: return BREAKOUT_STEP_MS;
        default:             return 200;
    }
}

void GameEngine::begin(Game g, uint32_t nowMs, uint32_t seed) {
    cur      = (g >= Game::COUNT) ? Game::SNAKE : g;
    rngState = seed ? seed : 0x1234567u;   // xorshift32 must never be seeded 0
    invRuns  = 0;                          // tallies are per-session, not lifetime
    invWins  = 0;
    dinoCrashCount = 0;
    reset(nowMs);
}

void GameEngine::reset(uint32_t nowMs) {
    lastStepMs = nowMs;
    steps      = 0;
    Draw::clear(buf);

    switch (cur) {
        case Game::SNAKE:    initSnake();    drawSnake();    break;
        case Game::INVADERS: initInvaders(); drawInvaders(); break;
        case Game::DINO:     initDino();     drawDino();     break;
        case Game::PONG:     initPong();     drawPong();     break;
        case Game::BREAKOUT: initBreakout(); drawBreakout(); break;
        default: break;
    }
}

void GameEngine::update(uint32_t nowMs) {
    const uint16_t interval = stepIntervalMs(cur);
    if ((uint32_t)(nowMs - lastStepMs) < interval) return;
    lastStepMs = nowMs;
    steps++;

    switch (cur) {
        case Game::SNAKE:    stepSnake();    drawSnake();    break;
        case Game::INVADERS: stepInvaders(); drawInvaders(); break;
        case Game::DINO:     stepDino();     drawDino();     break;
        case Game::PONG:     stepPong();     drawPong();     break;
        case Game::BREAKOUT: stepBreakout(); drawBreakout(); break;
        default: break;
    }
}

// ══ SNAKE ═════════════════════════════════════════════════════════════════════
//
// Greedy toward food with a 25% chance of a random legal move, which keeps the
// path from looking robotic. Walls are excluded from the legal set rather than
// being fatal, so the snake bumps and turns instead of dying.

void GameEngine::initSnake() {
    snLen = 3;
    snX[0] = 8; snY[0] = 8;
    snX[1] = 7; snY[1] = 8;
    snX[2] = 6; snY[2] = 8;
    snDX = 1; snDY = 0;

    // Place food anywhere not occupied by the body.
    for (;;) {
        foodX = rndN(16);
        foodY = rndN(16);
        bool clash = false;
        for (uint16_t i = 0; i < snLen; i++)
            if (snX[i] == foodX && snY[i] == foodY) { clash = true; break; }
        if (!clash) break;
    }
}

void GameEngine::stepSnake() {
    static const int8_t DX[4] = { 1, -1, 0,  0 };
    static const int8_t DY[4] = { 0,  0, 1, -1 };

    int8_t  candDir[4];
    int16_t candDist[4];
    uint8_t nCand = 0;

    for (uint8_t d = 0; d < 4; d++) {
        // Never reverse directly into the neck.
        if (DX[d] == -snDX && DY[d] == -snDY) continue;

        const int16_t nx = (int16_t)snX[0] + DX[d];
        const int16_t ny = (int16_t)snY[0] + DY[d];
        if (nx < 0 || nx > 15 || ny < 0 || ny > 15) continue;

        // The tail cell vacates this step, so it is safe to move into.
        bool hitsBody = false;
        for (uint16_t i = 0; i + 1 < snLen; i++)
            if (snX[i] == nx && snY[i] == ny) { hitsBody = true; break; }
        if (hitsBody) continue;

        candDir[nCand]  = (int8_t)d;
        candDist[nCand] = (int16_t)((nx > foodX ? nx - foodX : foodX - nx) +
                                    (ny > foodY ? ny - foodY : foodY - ny));
        nCand++;
    }

    if (nCand == 0) { initSnake(); return; }   // boxed in — soft respawn

    uint8_t pick;
    if (rndN(4) != 0) {
        // Greedy: choose uniformly among the moves tied for closest.
        int16_t best = candDist[0];
        for (uint8_t i = 1; i < nCand; i++) if (candDist[i] < best) best = candDist[i];

        uint8_t tied[4], nTied = 0;
        for (uint8_t i = 0; i < nCand; i++) if (candDist[i] == best) tied[nTied++] = i;
        pick = tied[rndN(nTied)];
    } else {
        pick = rndN(nCand);
    }

    const uint8_t d = (uint8_t)candDir[pick];
    snDX = DX[d];
    snDY = DY[d];

    const uint8_t nx = (uint8_t)((int16_t)snX[0] + snDX);
    const uint8_t ny = (uint8_t)((int16_t)snY[0] + snDY);
    const bool    ate = (nx == foodX && ny == foodY);

    // Shift the body back one cell, growing only when food was eaten.
    if (ate && snLen < SNAKE_MAX) snLen++;
    for (uint16_t i = snLen - 1; i > 0; i--) {
        snX[i] = snX[i - 1];
        snY[i] = snY[i - 1];
    }
    snX[0] = nx;
    snY[0] = ny;

    if (ate) {
        for (;;) {
            foodX = rndN(16);
            foodY = rndN(16);
            bool clash = false;
            for (uint16_t i = 0; i < snLen; i++)
                if (snX[i] == foodX && snY[i] == foodY) { clash = true; break; }
            if (!clash) break;
        }
    }
}

void GameEngine::drawSnake() {
    Draw::clear(buf);

    // Head bright mint fading to deep green at the tail.
    for (uint16_t i = 0; i < snLen; i++) {
        const uint16_t t = (snLen > 1) ? (uint16_t)(i * 255 / (snLen - 1)) : 0;
        const uint8_t  r = (uint8_t)(202 - (uint16_t)(202 - 31) * t / 255);
        const uint8_t  g = (uint8_t)(255 - (uint16_t)(255 - 156) * t / 255);
        const uint8_t  b = (uint8_t)(191 - (uint16_t)(191 - 80) * t / 255);
        Draw::px(buf, snX[i], snY[i], r, g, b);
    }

    Draw::px(buf, foodX, foodY, 255, 55, 95);
}

// ══ INVADERS ══════════════════════════════════════════════════════════════════
//
// A campaign of escalating levels rather than one endlessly repeating wave.
//
//   - Every level adds enemies (rows first, then a column), marches faster and
//     shoots more. Level 1 is 8 invaders firing 5% of steps; level 5 is 20
//     firing 22% of them. That ramp is the "overwhelmed eventually, not
//     immediately" the brief asks for.
//   - Every INV_BOSS_EVERY-th level replaces the formation with a mothership
//     that has HP, sweeps, dives and fires three-shot spreads.
//   - Clearing level INV_CAMPAIGN wins the run; taking a hit loses it. Either
//     way a short flourish plays and a fresh campaign starts, so play never
//     halts. A whole campaign is about 70 seconds, so the 20-second CYCLE ALL
//     slot shows a slice of one and an unattended panel shows many.
//
// A formation that marches all the way down does NOT end the run: it snaps back
// to the top as a fresh assault wave. Letting it be fatal would put a hard time
// limit on every level, and the limit would have to be retuned against the
// cannon's clear rate every time a formation size changed. Death comes from
// enemy fire and nothing else, which is one dial instead of two.
//
// THE 0.3 WIN RATE
// ----------------
// A screensaver has nobody to lose to, so "how often does the ship win?" has to
// be a decision, not an emergent property — an emergent one drifts every time a
// constant is retuned, and here it would have to be re-measured against every
// formation-size change. The outcome is therefore rolled once, at campaign
// start: with probability winChancePct the run is a hero run and every incoming
// shot is dodged; otherwise a doom level is drawn from the middle of the
// campaign, and on that level the ship stops dodging while the invaders start
// aiming at its column. Which invaders die when, how each boss fight goes and
// how long the run lasts are all still played out for real — only the verdict is
// pre-drawn, and a dodge is a visible sidestep rather than a bullet passing
// through the hull.

static const uint8_t INV_DODGE_ROWS = 5;   // how far up the ship watches for fire
static const int8_t  INV_BULLET_V   = 3;   // player bullet rows per step

void GameEngine::initInvaders() {
    invLevel   = 1;
    outcomeTtl = 0;
    heroRun    = rndN(100) < winChancePct;
    // A doomed run must not die on level 1 — that reads as "immediately
    // overwhelmed" rather than "eventually".
    doomLevel  = heroRun ? 0 : (uint8_t)(2 + rndN(INV_CAMPAIGN - 1));
    invadersLoadLevel();
}

void GameEngine::invadersLoadLevel() {
    invX     = 1;
    invY     = 0;
    invDir   = 1;
    invFrame = 0;
    invTick  = 0;
    bulX = bulY = -1;
    canX = 7;
    expX = expY = -1;
    expTtl = 0;
    bossFireTick = 0;
    dodgeTtl = 0;
    for (uint8_t i = 0; i < INV_EBUL_MAX; i++) { ebulX[i] = -1; ebulY[i] = -1; }

    // Fire density and march speed ramp with the level. The doom level gets a
    // further boost: the run is going to end here, and it should end because the
    // ship is visibly buried in fire rather than because one stray shot landed.
    invFirePct   = (uint8_t)(1 + invLevel * 4);
    if (invLevel == doomLevel) invFirePct = (uint8_t)(invFirePct + 25);
    invMoveEvery = (uint8_t)(invLevel >= 5 ? 2 : (invLevel >= 3 ? 3 : 4));

    if (invLevel % INV_BOSS_EVERY == 0) {
        invMask   = 0;
        invCols   = 0;
        invRows   = 0;
        bossHpMax = (uint8_t)(6 + 4 * (invLevel / INV_BOSS_EVERY));
        bossHp    = bossHpMax;
        bossX     = 4;
        bossY     = 1;
        bossDir   = 1;
        return;
    }

    bossHp  = 0;
    invRows = (uint8_t)(2 + invLevel / 2);
    if (invRows > INV_MAX_ROWS) invRows = INV_MAX_ROWS;
    invCols = (uint8_t)(invLevel >= 5 ? INV_MAX_COLS : 4);

    const uint8_t n = (uint8_t)(invCols * invRows);
    invMask = (uint32_t)((1u << n) - 1u);
}

void GameEngine::invaderPos(uint8_t i, int8_t& x, int8_t& y) const {
    x = (int8_t)(invX + (i % invCols) * INV_XPITCH);
    y = (int8_t)(invY + (i / invCols) * INV_YPITCH);
}

uint8_t GameEngine::invadersAlive() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < INV_MAX; i++) if (invMask & (1u << i)) n++;
    return n;
}

void GameEngine::invadersEndRun(bool won) {
    invRuns++;
    if (won) invWins++;
    outcomeWon = won;
    outcomeTtl = 18;                 // ~2s of flourish at INVADERS_STEP_MS
    bulX = bulY = -1;
    for (uint8_t i = 0; i < INV_EBUL_MAX; i++) { ebulX[i] = -1; ebulY[i] = -1; }
}

// The ship dodges every incoming shot except on the level the run is scripted to
// end on. A hero run has no doom level and so always dodges.
bool GameEngine::invadersShipShouldDodge() const {
    return invLevel != doomLevel;
}

void GameEngine::stepInvaders() {
    // ── Outcome flourish ──────────────────────────────────────────────────────
    if (outcomeTtl) {
        if (--outcomeTtl == 0) initInvaders();
        return;
    }
    if (dodgeTtl) dodgeTtl--;

    // ── Level complete? ───────────────────────────────────────────────────────
    if (bossHp == 0 && invMask == 0) {
        // Clearing the doom level does not save the run: the last enemy's
        // parting shot takes the ship with it. Without this the win rate is a
        // function of every formation size, fire rate and boss HP in the table —
        // measured at 0.315 rather than 0.300 — and would have to be re-measured
        // after any of them was retuned. With it, invadersWins()/invadersRuns()
        // converges on winChancePct exactly, whatever the levels look like.
        if (invLevel == doomLevel) {
            expX = canX; expY = 14; expTtl = 6;
            invadersEndRun(false);
            return;
        }
        if (invLevel >= INV_CAMPAIGN) { invadersEndRun(true); return; }
        invLevel++;
        invadersLoadLevel();
        return;
    }

    // ── March / boss movement ─────────────────────────────────────────────────
    if (++invTick >= invMoveEvery) {
        invTick  = 0;
        invFrame ^= 1;

        if (bossHp) {
            // The mothership sweeps, reversing at the walls, and creeps down a
            // row every full sweep so a stalled fight still closes the distance.
            const int8_t nx = (int8_t)(bossX + bossDir);
            if (nx < 0 || nx + 7 > 16) {
                bossDir = (int8_t)-bossDir;
                if (bossY < 5) bossY++;
            } else {
                bossX = nx;
            }
        } else {
            bool bounce = false;
            for (uint8_t i = 0; i < invCols * invRows; i++) {
                if (!(invMask & (1u << i))) continue;
                int8_t x, y;
                invaderPos(i, x, y);
                const int8_t nx = (int8_t)(x + invDir);
                if (nx < 0 || nx + 1 > 15) { bounce = true; break; }
            }

            if (bounce) {
                invDir = (int8_t)-invDir;
                invY   = (int8_t)(invY + 1);
            } else {
                invX = (int8_t)(invX + invDir);
            }

            // Reaching the deck recycles the formation to the top as a fresh
            // assault wave rather than ending the run (see the header note).
            int8_t lowest = -1;
            for (uint8_t i = 0; i < invCols * invRows; i++) {
                if (!(invMask & (1u << i))) continue;
                int8_t x, y;
                invaderPos(i, x, y);
                if (y > lowest) lowest = y;
            }
            if (lowest + 1 >= 12) { invY = 0; waveFlash = 4; }
        }
    }
    if (waveFlash) waveFlash--;

    // ── Cannon: aim at the lowest live target ─────────────────────────────────
    int8_t targetX = canX;
    if (bossHp) {
        targetX = (int8_t)(bossX + 3);
    } else {
        int8_t lowest = -1;
        for (uint8_t i = 0; i < invCols * invRows; i++) {
            if (!(invMask & (1u << i))) continue;
            int8_t x, y;
            invaderPos(i, x, y);
            if (y > lowest) { lowest = y; targetX = x; }
        }
    }
    if (canX < targetX) canX++;
    else if (canX > targetX) canX--;
    if (canX < 1)  canX = 1;
    if (canX > 14) canX = 14;

    // ── Player bullet ─────────────────────────────────────────────────────────
    if (bulY >= 0) {
        bulY = (int8_t)(bulY - INV_BULLET_V);
        if (bulY < 0) { bulX = bulY = -1; }
        else if (bossHp) {
            if (bulX >= bossX && bulX <= bossX + 6 && bulY >= bossY && bulY <= bossY + 3) {
                bossHp--;
                expX = bulX; expY = bulY; expTtl = 3;
                bulX = bulY = -1;
                if (bossHp == 0) {
                    if (invLevel == doomLevel) {
                        expX = canX; expY = 14; expTtl = 6;
                        invadersEndRun(false);
                        return;
                    }
                    if (invLevel >= INV_CAMPAIGN) { invadersEndRun(true); return; }
                    invLevel++;
                    invadersLoadLevel();
                    return;
                }
            }
        } else {
            for (uint8_t i = 0; i < invCols * invRows; i++) {
                if (!(invMask & (1u << i))) continue;
                int8_t x, y;
                invaderPos(i, x, y);
                // Hitbox is widened by a pixel each side and a row below: the
                // formation drifts during the bullet's flight and the bullet
                // moves INV_BULLET_V rows a step, so an exact test would skip
                // straight past most targets.
                if (bulX >= x - 1 && bulX <= x + 2 &&
                    bulY >= y - 1 && bulY <= y + INV_BULLET_V) {
                    invMask = (uint32_t)(invMask & ~(1u << i));
                    expX = x; expY = y; expTtl = 3;
                    bulX = bulY = -1;
                    break;
                }
            }
        }
    }
    // Reload as soon as the barrel is clear: at INV_BULLET_V rows a step that is
    // a shot roughly every five steps, which is what makes a 20-invader level
    // clearable inside a minute.
    if (bulY < 0) { bulX = canX; bulY = 13; }

    // ── Enemy fire ────────────────────────────────────────────────────────────
    for (uint8_t i = 0; i < INV_EBUL_MAX; i++) {
        if (ebulY[i] < 0) continue;
        ebulY[i] = (int8_t)(ebulY[i] + 1);
        if (ebulY[i] > 15) { ebulX[i] = -1; ebulY[i] = -1; continue; }

        // The deck is rows 14-15 and the cannon is three pixels wide.
        if (ebulY[i] >= 14 && ebulX[i] >= canX - 1 && ebulX[i] <= canX + 1) {
            if (!invadersShipShouldDodge()) {
                expX = canX; expY = 14; expTtl = 6;
                invadersEndRun(false);
                return;
            }
            // Dodged: the shot is spent on the deck beside the ship and the ship
            // is thrown clear. Drawing the sidestep is what keeps this from
            // looking like a bullet passing through the hull.
            int8_t away = (ebulX[i] <= canX) ? 2 : -2;
            if (canX + away < 1 || canX + away > 14) away = (int8_t)-away;
            dodgeX   = ebulX[i];
            dodgeTtl = 3;
            canX     = (int8_t)(canX + away);
            if (canX < 1)  canX = 1;
            if (canX > 14) canX = 14;
            ebulX[i] = -1; ebulY[i] = -1;
        }
    }

    if (rndN(100) < invFirePct) {
        if (bossHp) {
            if (++bossFireTick >= 2) {
                bossFireTick = 0;
                // A three-shot spread, which is what makes a boss feel different
                // from a column of invaders. On the doom level the spread is
                // centred on the ship instead of on the hull, because a hull-
                // relative spread systematically misses a cannon that is
                // tracking the same sweeping boss.
                const int8_t base = (invLevel == doomLevel) ? (int8_t)(canX - 3)
                                                            : bossX;
                static const int8_t OFF[3] = { 0, 3, 6 };
                for (uint8_t k = 0; k < 3; k++)
                    for (uint8_t s = 0; s < INV_EBUL_MAX; s++)
                        if (ebulY[s] < 0) {
                            ebulX[s] = (int8_t)(base + OFF[k]);
                            ebulY[s] = (int8_t)(bossY + 4);
                            break;
                        }
            }
        } else if (invCols) {
            // Only the lowest invader in a column may shoot — otherwise fire
            // rains out of the middle of the formation and reads as noise. On
            // the doom level the column is chosen for being over the ship
            // instead of at random, so the run ends under aimed fire.
            uint8_t col = rndN(invCols);
            if (invLevel == doomLevel) {
                int8_t best = 127;
                for (uint8_t c = 0; c < invCols; c++) {
                    const int8_t cx = (int8_t)(invX + c * INV_XPITCH);
                    const int8_t d  = (int8_t)(cx > canX ? cx - canX : canX - cx);
                    if (d < best) { best = d; col = c; }
                }
            }
            for (int8_t r = (int8_t)invRows - 1; r >= 0; r--) {
                const uint8_t idx = (uint8_t)(r * invCols + col);
                if (!(invMask & (1u << idx))) continue;
                int8_t x, y;
                invaderPos(idx, x, y);
                for (uint8_t s = 0; s < INV_EBUL_MAX; s++)
                    if (ebulY[s] < 0) { ebulX[s] = x; ebulY[s] = (int8_t)(y + 2); break; }
                break;
            }
        }
    }

    if (expTtl) expTtl--;
    else        { expX = expY = -1; }
}

void GameEngine::drawInvaders() {
    Draw::clear(buf);

    // Deep space backdrop keeps the panel from looking switched off.
    Draw::fill(buf, 0, 0, 6);

    // ── Outcome flourish ──────────────────────────────────────────────────────
    if (outcomeTtl) {
        const uint8_t phase = (uint8_t)(18 - outcomeTtl);
        if (outcomeWon) {
            // A victory sweep rising up the panel, with the ship left standing.
            for (uint8_t y = 0; y < 16; y++) {
                const int16_t d = (int16_t)(15 - y) - (int16_t)phase;
                if (d < 0 || d > 3) continue;
                const uint8_t v = (uint8_t)(255 - d * 60);
                Draw::rect(buf, 0, y, 16, 1, (uint8_t)(v / 6), v, (uint8_t)(v / 3));
            }
            Draw::px(buf, canX,     15, 90, 255, 140);
            Draw::px(buf, canX - 1, 15, 60, 200, 110);
            Draw::px(buf, canX + 1, 15, 60, 200, 110);
            Draw::px(buf, canX,     14, 160, 255, 190);
        } else {
            // The ship goes up: an expanding ring of debris where it stood.
            const uint8_t rad = (uint8_t)(phase / 2);
            for (int16_t dy = -(int16_t)rad; dy <= (int16_t)rad; dy++)
                for (int16_t dx = -(int16_t)rad; dx <= (int16_t)rad; dx++) {
                    const int16_t m = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                    if (m != (int16_t)rad) continue;
                    const uint8_t v = (uint8_t)(255 - phase * 12);
                    Draw::px(buf, (int16_t)(canX + dx), (int16_t)(14 + dy),
                             v, (uint8_t)(v / 2), 0);
                }
        }
        return;
    }

    // A recycled formation flashes the top row, so a wave arriving from nowhere
    // is announced rather than just appearing.
    if (waveFlash) {
        const uint8_t v = (uint8_t)(waveFlash * 40);
        Draw::rect(buf, 0, 0, 16, 1, v, (uint8_t)(v / 4), (uint8_t)(v / 2));
    }

    // ── Boss ──────────────────────────────────────────────────────────────────
    if (bossHp) {
        // Colour tracks remaining HP: violet when fresh, red when nearly dead,
        // so the fight's progress is legible without a health bar.
        const uint8_t hpFrac = (uint8_t)((uint16_t)bossHp * 255 / bossHpMax);
        const uint8_t br = (uint8_t)(255 - hpFrac / 3);
        const uint8_t bg = (uint8_t)(40 + hpFrac / 4);
        const uint8_t bb = (uint8_t)(60 + hpFrac / 2);

        // 7x4 mothership: stepped shoulders, a wide hull and trailing engines.
        static const uint8_t HULL[4] = { 0x1C, 0x7F, 0x7F, 0x36 };   // bit c = x0+c
        for (uint8_t row = 0; row < 4; row++)
            for (uint8_t c = 0; c < 7; c++)
                if (HULL[row] & (1u << c))
                    Draw::px(buf, (int16_t)(bossX + c), (int16_t)(bossY + row), br, bg, bb);

        // Core pulses with the wiggle phase — the only bright thing on the hull.
        const uint8_t core = invFrame ? 255 : 140;
        Draw::px(buf, (int16_t)(bossX + 3), (int16_t)(bossY + 1), core, core, (uint8_t)(core / 3));
        Draw::px(buf, (int16_t)(bossX + 3), (int16_t)(bossY + 2), core, (uint8_t)(core / 2), 0);
    }

    // ── Formation ─────────────────────────────────────────────────────────────
    // Row colour walks the hue wheel so a five-row formation still has five
    // distinguishable bands.
    for (uint8_t i = 0; i < invCols * invRows; i++) {
        if (!(invMask & (1u << i))) continue;
        int8_t x, y;
        invaderPos(i, x, y);
        const uint8_t row = (uint8_t)(i / invCols);
        const RGB8 c = ColorUtils::hsv2rgb((uint8_t)(row * 40), 220, 255);

        Draw::px(buf, x,     y, c.r, c.g, c.b);
        Draw::px(buf, x + 1, y, c.r, c.g, c.b);
        // A single alternating "leg" pixel reads as movement at this scale and
        // fits the 2px row pitch the denser formations need.
        Draw::px(buf, (int16_t)(invFrame ? x : x + 1), (int16_t)(y + 1),
                 (uint8_t)(c.r / 2), (uint8_t)(c.g / 2), (uint8_t)(c.b / 2));
    }

    if (expTtl && expX >= 0) {
        Draw::px(buf, expX,     expY,     255, 255, 200);
        Draw::px(buf, expX + 1, expY,     255, 200, 100);
        Draw::px(buf, expX,     expY + 1, 255, 200, 100);
        Draw::px(buf, expX + 1, expY + 1, 255, 255, 200);
    }

    for (uint8_t i = 0; i < INV_EBUL_MAX; i++)
        if (ebulY[i] >= 0) Draw::px(buf, ebulX[i], ebulY[i], 255, 80, 40);

    if (bulY >= 0) Draw::px(buf, bulX, bulY, 255, 255, 255);

    // A dodged shot bursts on the deck where the ship used to be.
    if (dodgeTtl)
        Draw::px(buf, dodgeX, 15, 255, (uint8_t)(dodgeTtl * 60), 40);

    // Cannon: a three-pixel base with a muzzle.
    Draw::px(buf, canX,     15, 90, 255, 140);
    Draw::px(buf, canX - 1, 15, 60, 200, 110);
    Draw::px(buf, canX + 1, 15, 60, 200, 110);
    Draw::px(buf, canX,     14, 160, 255, 190);
}

// ══ DINO ══════════════════════════════════════════════════════════════════════
//
// Endless runner with a proper T-Rex silhouette and a jump that is *planned*
// rather than triggered by a hand-tuned distance window.
//
// THE OLD BUG
// -----------
// The previous version had no collision detection at all, and took off whenever
// an obstacle fell inside a speed-relative gap. That window was derived for a
// 3px-wide dino; the arc's useful clearance lasted only four steps, so at the
// slow end of the speed ramp a cactus was still under the dino as it came down.
// The dino visibly landed on the cactus and ran straight through it.
//
// THE FIX
// -------
// 1. The arc is longer and higher (JUMP_V0/JUMP_GRAV give a 15-step flight
//    peaking at 7.7px instead of a 9-step one peaking at 5.2px), so there is
//    slack to work with at every speed.
// 2. Takeoff is decided by simulating the whole arc against the actual obstacle
//    positions: the dino jumps on the last step from which the simulated flight
//    is collision-free. dinoArcSafe() is the planner; dinoWouldHit() is the same
//    predicate the real collision check uses, so the plan cannot disagree with
//    the outcome.
// 3. A real collision check still runs every step. It should never fire — the
//    soak test asserts dinoCrashes() stays 0 — but if it ever does, the dino
//    stumbles and the run resets rather than phasing through the cactus.
//
// The planner's hit box is the sprite's body (x1..x5), not its bounding box: the
// tail tip and the snout are one-pixel protrusions and clipping them would cost
// more clearance than it buys legibility.

static const uint8_t DINO_HIT_X0 = 1;
static const uint8_t DINO_HIT_W  = 5;
static const uint8_t DINO_PLAN_MARGIN = 1;   // px of slack the planner demands

void GameEngine::initDino() {
    dinoY   = 0;
    dinoVY  = 0;
    for (uint8_t i = 0; i < OBS_MAX; i++) { obsX[i] = -1; obsH[i] = 0; obsW[i] = 0; }
    dinoSpeed   = 10;
    dinoDist    = 0;
    dinoStumble = 0;
    night       = false;
}

// Does the dino's body box overlap an obstacle, with the dino `heightT` tenths
// of a pixel off the ground and every obstacle displaced by `shift` tenths?
// `margin` grows the obstacles by that many pixels on each side; the planner
// passes 1 and the real collision check passes 0, so the plan always leaves a
// pixel of slack for the speed ramp ticking up mid-flight.
bool GameEngine::dinoWouldHit(int16_t heightT, int16_t shift, uint8_t margin) const {
    const int16_t h  = heightT / 10;                // whole pixels off the ground
    const int16_t x0 = (int16_t)DINO_HIT_X0;
    const int16_t x1 = (int16_t)(DINO_HIT_X0 + DINO_HIT_W - 1);

    for (uint8_t i = 0; i < OBS_MAX; i++) {
        if (obsX[i] < 0) continue;
        if (h >= (int16_t)obsH[i]) continue;        // clears it vertically
        const int16_t px = floorDiv10((int16_t)(obsX[i] + shift));
        if (px + (int16_t)obsW[i] - 1 + (int16_t)margin < x0) continue;
        if (px - (int16_t)margin > x1) continue;
        return true;
    }
    return false;
}

// Simulate `delay` further grounded steps followed by a full jump, and report
// whether every step of that plan clears every obstacle. Obstacles are assumed
// to keep the current speed, which is what DINO_PLAN_MARGIN covers.
bool GameEngine::dinoArcSafe(uint8_t delay) const {
    const int16_t v = (int16_t)dinoSpeed;

    for (uint8_t k = 1; k <= delay; k++)
        if (dinoWouldHit(0, (int16_t)(-(int16_t)k * v), DINO_PLAN_MARGIN)) return false;

    int16_t y = 0, vy = JUMP_V0;
    for (uint8_t j = 1; j <= DINO_FLIGHT_STEPS; j++) {
        y  = (int16_t)(y + vy);
        vy = (int16_t)(vy - JUMP_GRAV);
        if (y < 0) y = 0;
        const int16_t shift = (int16_t)(-(int16_t)(delay + j) * v);
        if (dinoWouldHit(y, shift, DINO_PLAN_MARGIN)) return false;
        if (y == 0) break;                          // landed
    }
    return true;
}

// Would staying grounded run the dino into something within one flight's worth
// of steps? That horizon is what turns the planner on; outside it the dino has
// no reason to jump yet.
bool GameEngine::dinoCrashComing() const {
    for (uint8_t k = 1; k <= DINO_FLIGHT_STEPS; k++)
        if (dinoWouldHit(0, (int16_t)(-(int16_t)k * (int16_t)dinoSpeed), DINO_PLAN_MARGIN))
            return true;
    return false;
}

void GameEngine::stepDino() {
    // ── Crash flourish ────────────────────────────────────────────────────────
    if (dinoStumble) {
        if (--dinoStumble == 0) initDino();
        return;
    }

    dinoDist++;
    if (dinoDist % 340 == 0) night = !night;

    const bool grounded = (dinoY == 0 && dinoVY == 0);

    // Speed ramps then resets, so the pace visibly builds and restarts — but it
    // only ever changes on an empty track. The takeoff planner simulates the
    // whole flight at the current speed, and a ramp tick partway through an
    // approach silently invalidates a plan that was correct when it was made:
    // that is the second, subtler half of the collision bug this rework fixes.
    // A soak with the ramp free-running produced roughly one crash per 22,000
    // steps, all of them on the step the speed changed.
    bool trackClear = true;
    for (uint8_t i = 0; i < OBS_MAX; i++) if (obsX[i] >= 0) { trackClear = false; break; }
    if (trackClear && grounded)
        dinoSpeed = (uint16_t)(10 + (dinoDist % 900) / 90);    // 1.0 .. 1.9 px/step

    // ── Obstacles ─────────────────────────────────────────────────────────────
    // obsX < 0 is the single source of truth for "slot free". An earlier version
    // also kept an obsUsed counter, which leaked: a slot at -10 was free for
    // spawning but had not yet crossed the despawn threshold, so it was recycled
    // without ever being decremented and spawning stopped once it reached OBS_MAX.
    uint8_t live = 0;
    int16_t rightmost = -1000;

    for (uint8_t i = 0; i < OBS_MAX; i++) {
        if (obsX[i] < 0) continue;
        obsX[i] = (int16_t)(obsX[i] - (int16_t)dinoSpeed);
        if (obsX[i] < -40) { obsX[i] = -1; obsH[i] = 0; obsW[i] = 0; continue; }
        live++;
        if (obsX[i] > rightmost) rightmost = obsX[i];
    }

    // Spawning is only allowed while grounded and with a full flight's worth of
    // clear track behind the last obstacle. Both rules exist for the planner: it
    // simulates against the obstacles it can see, so an obstacle that appears
    // mid-flight would invalidate a plan already committed to.
    if (grounded && live < OBS_MAX && rndN(100) < 14) {
        const int16_t spawnX = (int16_t)(170 + rndN(50));
        if (rightmost < (int16_t)(spawnX - 175)) {
            for (uint8_t i = 0; i < OBS_MAX; i++) {
                if (obsX[i] >= 0) continue;
                obsX[i] = spawnX;
                // Shapes are paired so the widest obstacle is also the lowest:
                // clearance time falls as the cactus grows and rises as it
                // widens, and this keeps every combination inside the arc.
                switch (rndN(4)) {
                    case 0: obsW[i] = 1; obsH[i] = 4; break;   // tall spire
                    case 1: obsW[i] = 2; obsH[i] = 3; break;   // classic cactus
                    case 2: obsW[i] = 3; obsH[i] = 2; break;   // low cluster
                    default: obsW[i] = 1; obsH[i] = 3; break;  // sapling
                }
                break;
            }
        }
    }

    // ── Takeoff planning ──────────────────────────────────────────────────────
    if (grounded && dinoCrashComing()) {
        // Jump on the last step from which the arc is still clean. Taking off at
        // the earliest safe step instead would launch the dino while the cactus
        // is still off-screen, and it would land in front of it.
        if (dinoArcSafe(0) && !dinoArcSafe(1)) dinoVY = JUMP_V0;
    }

    if (dinoVY != 0 || dinoY > 0) {
        dinoY  = (int16_t)(dinoY + dinoVY);
        dinoVY = (int16_t)(dinoVY - JUMP_GRAV);
        if (dinoY <= 0) { dinoY = 0; dinoVY = 0; }
    }

    // ── Collision ─────────────────────────────────────────────────────────────
    // The planner is supposed to make this unreachable; it is here so that a
    // planning failure looks like a stumble rather than a dino running through
    // solid scenery.
    if (dinoWouldHit(dinoY, 0, 0)) {
        dinoCrashCount++;
        dinoStumble = 10;
    }
}

void GameEngine::drawDino() {
    // The background stays BLACK in both palettes. The original "day" mode lit
    // every pixel with a pale sky and drew a dark grey dino on top; on a real
    // WS2812B panel that reads badly, because two lit colours of similar
    // lightness are much harder to tell apart than a lit colour against unlit
    // LEDs. Black is not a colour here — it is switched-off pixels, and it is
    // the strongest contrast the panel has.
    //
    // Day and night differ in the palette of what is *drawn*, not in what is
    // behind it. The dino is warm green and the cacti are a cool blue-green: two
    // greens, but far enough apart in hue and lightness to separate the runner
    // from the scenery even when they overlap on the same rows.
    const uint8_t fgR = night ?  90 : 165, fgG = night ? 190 : 235, fgB = night ? 150 :  90;
    const uint8_t grR = night ?  55 : 165, grG = night ?  70 : 140, grB = night ? 105 :  70;
    const uint8_t caR = night ?  30 :  40, caG = night ? 120 : 165, caB = night ? 140 : 145;

    Draw::clear(buf);

    // Ground line plus a scrolling speckle so motion is visible even with no
    // obstacle on screen.
    Draw::rect(buf, 0, (int16_t)(DINO_GROUND_Y + 1), 16, 1, grR, grG, grB);
    for (uint8_t x = 0; x < 16; x++)
        if (((x + (dinoDist / 2)) % 5) == 0)
            Draw::px(buf, x, 15, (uint8_t)(grR / 2), (uint8_t)(grG / 2), (uint8_t)(grB / 2));

    // Obstacles: cacti standing on the ground line. A width-2 or width-3 cactus
    // gets a shorter outer column, which is what makes it read as arms rather
    // than as a wall.
    for (uint8_t i = 0; i < OBS_MAX; i++) {
        if (obsX[i] < 0) continue;
        const int16_t px = floorDiv10(obsX[i]);
        if (px + (int16_t)obsW[i] < 0 || px > 16) continue;
        for (uint8_t c = 0; c < obsW[i]; c++) {
            const bool centre = (obsW[i] == 3) ? (c == 1) : (c == 0);
            const uint8_t h = centre ? obsH[i] : (uint8_t)(obsH[i] > 1 ? obsH[i] - 1 : 1);
            for (uint8_t k = 0; k < h; k++)
                Draw::px(buf, (int16_t)(px + c), (int16_t)(DINO_GROUND_Y - k), caR, caG, caB);
        }
    }

    // ── T-Rex ─────────────────────────────────────────────────────────────────
    // 7x6, facing right. Bit c of each row is column c (0 = leftmost):
    //
    //     . . . . # # #     head crown
    //     . . . . # # #     head + eye
    //     # . . # # # .     raised tail tip, jaw, neck
    //     . # # # # # .     back sloping down from the tail
    //     . . # # # . .     belly
    //     . . L . R . .     legs (alternate while running)
    //
    // What makes it read as a T-Rex at this size is the combination of a heavy
    // block head high on the right, a tail tip held clear of the back at the far
    // left, and two separated legs. Dropping any one of the three turns it back
    // into an anonymous blob — an earlier pass had the tail flush with the back
    // and the whole thing read as a lizard-shaped brick.
    static const uint8_t TREX[5] = { 0x70, 0x70, 0x39, 0x3E, 0x1C };

    const int16_t by = (int16_t)(DINO_GROUND_Y - dinoY / 10);   // feet row
    const int16_t ty = (int16_t)(by - (int16_t)DINO_H + 1);     // sprite top row

    for (uint8_t row = 0; row < 5; row++)
        for (uint8_t c = 0; c < DINO_W; c++)
            if (TREX[row] & (1u << c))
                Draw::px(buf, (int16_t)(DINO_X + c), (int16_t)(ty + row), fgR, fgG, fgB);

    // Legs: planted apart on one frame and together on the next. In the air both
    // are tucked forward, which is the classic runner pose.
    if (dinoY == 0) {
        if ((dinoDist / 3) % 2) {
            Draw::px(buf, (int16_t)(DINO_X + 2), by, fgR, fgG, fgB);
            Draw::px(buf, (int16_t)(DINO_X + 4), by, fgR, fgG, fgB);
        } else {
            Draw::px(buf, (int16_t)(DINO_X + 3), by, fgR, fgG, fgB);
            Draw::px(buf, (int16_t)(DINO_X + 4), by, fgR, fgG, fgB);
        }
    } else {
        Draw::px(buf, (int16_t)(DINO_X + 3), by, fgR, fgG, fgB);
        Draw::px(buf, (int16_t)(DINO_X + 4), by, fgR, fgG, fgB);
    }

    // Eye: a bright pixel inside the head block. It is the one detail that turns
    // the head from a rectangle into a face.
    Draw::px(buf, (int16_t)(DINO_X + 5), (int16_t)(ty + 1), 255, 255, 255);

    // Crash flourish — the dino flashes red where it stands.
    if (dinoStumble) {
        const uint8_t v = (uint8_t)(dinoStumble * 25);
        for (uint8_t row = 0; row < 5; row++)
            for (uint8_t c = 0; c < DINO_W; c++)
                if (TREX[row] & (1u << c))
                    Draw::px(buf, (int16_t)(DINO_X + c), (int16_t)(ty + row), 255, v, v);
    }
}

// ══ PONG ══════════════════════════════════════════════════════════════════════
//
// Two AI paddles rallying with up to three balls at once. Positions are /16 px
// so the ball can carry shallow angles: on a 16px field, integer velocities can
// only express 45-degree diagonals and the rally looks like a metronome.
//
// Each ball owns a hue that advances every step and drags a four-sample trail,
// so a single ball paints a moving rainbow rather than a white dot. Paddle hues
// drift too, and a paddle's colour is what its side's score pips are drawn in.
//
// The paddles are deliberately imperfect: each carries a signed aim error that
// is redrawn on every return, so rallies build and then break instead of running
// forever.

static const int16_t PONG_MIN_X = 1 * 16;
static const int16_t PONG_MAX_X = 14 * 16;
static const int16_t PONG_MAX_Y = 15 * 16;
static const int16_t PONG_PAD_SPEED = 11;      // /16 px per step

void GameEngine::pongServe(uint8_t i, int8_t dir) {
    pbX[i]  = 7 * 16 + (int16_t)rndN(32);
    pbY[i]  = (int16_t)(3 * 16 + rndN(9 * 16));
    pbVX[i] = (int16_t)(dir * (10 + (int16_t)rndN(4)));
    pbVY[i] = (int16_t)((rndN(2) ? 1 : -1) * (5 + (int16_t)rndN(7)));
    pbHue[i]  = rndN(255);
    pbSpin[i] = (uint8_t)(2 + rndN(5));
    for (uint8_t t = 0; t < PONG_TRAIL; t++) {
        pbTrX[i][t] = (int8_t)(pbX[i] / 16);
        pbTrY[i][t] = (int8_t)(pbY[i] / 16);
    }
}

void GameEngine::pongSetBallCount(uint8_t n) {
    if (n > PONG_BALLS) n = PONG_BALLS;
    while (pbLive < n) { pongServe(pbLive, rndN(2) ? 1 : -1); pbLive++; }
    if (n < pbLive) pbLive = n;
}

void GameEngine::initPong() {
    pbLive   = 1;
    pgScoreL = pgScoreR = 0;
    pgRally  = 0;
    pgFlash  = 0;
    padLY = padRY = (int16_t)((16 - PONG_PAD_H) / 2 * 16);
    padLHue = rndN(255);
    padRHue = (uint8_t)(padLHue + 100);
    padLErr = (int8_t)(((int8_t)rndN(11) - 5) * 4);
    padRErr = (int8_t)(((int8_t)rndN(11) - 5) * 4);
    pongServe(0, rndN(2) ? 1 : -1);
}

void GameEngine::stepPong() {
    if (pgFlash) pgFlash--;
    padLHue = (uint8_t)(padLHue + 1);
    padRHue = (uint8_t)(padRHue + 1);

    // ── Paddles ───────────────────────────────────────────────────────────────
    // Each paddle tracks the ball closing on it soonest; with no ball inbound it
    // drifts back to the middle so the next serve is not a free point.
    for (uint8_t side = 0; side < 2; side++) {
        int16_t  target = (int16_t)(8 * 16);
        int16_t  bestETA = 32767;
        for (uint8_t i = 0; i < pbLive; i++) {
            const bool inbound = side ? (pbVX[i] > 0) : (pbVX[i] < 0);
            if (!inbound) continue;
            const int16_t dist = side ? (PONG_MAX_X - pbX[i]) : (pbX[i] - PONG_MIN_X);
            const int16_t vx   = pbVX[i] < 0 ? (int16_t)-pbVX[i] : pbVX[i];
            const int16_t eta  = vx ? (int16_t)(dist / vx) : 32767;
            if (eta < bestETA) { bestETA = eta; target = pbY[i]; }
        }

        // The aim error is in /16 px, not whole pixels: a whole-pixel error on a
        // 4px paddle missed about a third of returns, so rallies averaged under
        // three and the second and third balls were effectively never reached.
        const int8_t err = side ? padRErr : padLErr;
        int16_t want = (int16_t)(target - (PONG_PAD_H / 2) * 16 + (int16_t)err);
        if (want < 0) want = 0;
        if (want > (int16_t)((16 - PONG_PAD_H) * 16)) want = (int16_t)((16 - PONG_PAD_H) * 16);

        int16_t& y = side ? padRY : padLY;
        if (y < want) y = (int16_t)(y + (want - y > PONG_PAD_SPEED ? PONG_PAD_SPEED : want - y));
        else if (y > want) y = (int16_t)(y - (y - want > PONG_PAD_SPEED ? PONG_PAD_SPEED : y - want));
    }

    // ── Balls ─────────────────────────────────────────────────────────────────
    for (uint8_t i = 0; i < pbLive; i++) {
        for (uint8_t t = PONG_TRAIL - 1; t > 0; t--) {
            pbTrX[i][t] = pbTrX[i][t - 1];
            pbTrY[i][t] = pbTrY[i][t - 1];
        }
        pbTrX[i][0] = (int8_t)(pbX[i] / 16);
        pbTrY[i][0] = (int8_t)(pbY[i] / 16);

        pbHue[i] = (uint8_t)(pbHue[i] + pbSpin[i]);
        pbX[i]   = (int16_t)(pbX[i] + pbVX[i]);
        pbY[i]   = (int16_t)(pbY[i] + pbVY[i]);

        if (pbY[i] < 0)           { pbY[i] = (int16_t)-pbY[i];                     pbVY[i] = (int16_t)-pbVY[i]; }
        if (pbY[i] > PONG_MAX_Y)  { pbY[i] = (int16_t)(2 * PONG_MAX_Y - pbY[i]);   pbVY[i] = (int16_t)-pbVY[i]; }

        int8_t conceded = 0;   // -1 left conceded, +1 right conceded
        if (pbX[i] <= PONG_MIN_X && pbVX[i] < 0) {
            const int16_t rel = (int16_t)(pbY[i] - padLY);
            if (rel >= -8 && rel <= (int16_t)(PONG_PAD_H * 16)) {
                pbX[i]   = PONG_MIN_X;
                pbVX[i]  = (int16_t)-pbVX[i];
                // Angle off the paddle depends on where it was struck, which is
                // what stops a rally settling into one repeating path.
                pbVY[i]  = (int16_t)(pbVY[i] + (rel - (PONG_PAD_H * 16) / 2) / 6);
                padLErr  = (int8_t)(((int8_t)rndN(11) - 5) * 4);
                if (pgRally < 255) pgRally++;
            } else conceded = -1;
        } else if (pbX[i] >= PONG_MAX_X && pbVX[i] > 0) {
            const int16_t rel = (int16_t)(pbY[i] - padRY);
            if (rel >= -8 && rel <= (int16_t)(PONG_PAD_H * 16)) {
                pbX[i]   = PONG_MAX_X;
                pbVX[i]  = (int16_t)-pbVX[i];
                pbVY[i]  = (int16_t)(pbVY[i] + (rel - (PONG_PAD_H * 16) / 2) / 6);
                padRErr  = (int8_t)(((int8_t)rndN(11) - 5) * 4);
                if (pgRally < 255) pgRally++;
            } else conceded = 1;
        }

        // Keep the vertical component inside a band: too flat and the ball
        // crawls along a wall, too steep and it is all bounce and no rally.
        if (pbVY[i] > 12)  pbVY[i] = 12;
        if (pbVY[i] < -12) pbVY[i] = -12;
        if (pbVY[i] > -2 && pbVY[i] < 2) pbVY[i] = (int16_t)(pbVY[i] >= 0 ? 3 : -3);

        if (conceded) {
            if (conceded < 0) pgScoreR++; else pgScoreL++;
            pgFlash    = 5;
            pgFlashHue = (uint8_t)(conceded < 0 ? padRHue : padLHue);
            pgRally    = 0;

            // Remove this ball by swapping the last one down.
            pbLive--;
            if (i < pbLive) {
                pbX[i] = pbX[pbLive]; pbY[i] = pbY[pbLive];
                pbVX[i] = pbVX[pbLive]; pbVY[i] = pbVY[pbLive];
                pbHue[i] = pbHue[pbLive]; pbSpin[i] = pbSpin[pbLive];
                memcpy(pbTrX[i], pbTrX[pbLive], PONG_TRAIL);
                memcpy(pbTrY[i], pbTrY[pbLive], PONG_TRAIL);
            }
            i--;                                  // re-examine the swapped ball
            if (pbLive == 0) { pongServe(0, conceded < 0 ? 1 : -1); pbLive = 1; }

            // A match runs to 7; then everything resets, hues included, so the
            // panel is never the same two colours for long.
            if (pgScoreL >= 5 || pgScoreR >= 5) { initPong(); return; }
        }
    }

    // A sustained rally earns another ball — the screen gets busier the better
    // the paddles are playing, and three trails at once is the whole point of
    // the multicoloured ball.
    if (pgRally >= 4  && pbLive < 2) pongSetBallCount(2);
    if (pgRally >= 9  && pbLive < 3) pongSetBallCount(3);
}

void GameEngine::drawPong() {
    Draw::clear(buf);

    // Score pips along the bottom edge, dim enough to read as furniture.
    for (uint8_t s = 0; s < pgScoreL && s < 5; s++) {
        const RGB8 c = ColorUtils::hsv2rgb(padLHue, 200, 50);
        Draw::px(buf, (int16_t)(1 + s), 15, c.r, c.g, c.b);
    }
    for (uint8_t s = 0; s < pgScoreR && s < 5; s++) {
        const RGB8 c = ColorUtils::hsv2rgb(padRHue, 200, 50);
        Draw::px(buf, (int16_t)(14 - s), 15, c.r, c.g, c.b);
    }

    // Paddles, each a short vertical gradient in its own hue.
    for (uint8_t k = 0; k < PONG_PAD_H; k++) {
        const uint8_t v = (uint8_t)(255 - k * 30);
        const RGB8 l = ColorUtils::hsv2rgb((uint8_t)(padLHue + k * 6), 220, v);
        const RGB8 r = ColorUtils::hsv2rgb((uint8_t)(padRHue + k * 6), 220, v);
        Draw::px(buf, 0,  (int16_t)(padLY / 16 + k), l.r, l.g, l.b);
        Draw::px(buf, 15, (int16_t)(padRY / 16 + k), r.r, r.g, r.b);
    }

    // Trails, oldest sample first so the ball itself always wins the pixel. The
    // floor matters: a trail that fades to 48 disappears entirely once the hue
    // lands anywhere blue, and pong lights only about 5% of the panel to start
    // with — there is nothing else on screen to carry the frame.
    for (uint8_t i = 0; i < pbLive; i++) {
        for (uint8_t t = PONG_TRAIL; t > 0; t--) {
            const uint8_t k = (uint8_t)(t - 1);
            const uint8_t v = (uint8_t)(200 - k * 40);
            // A wide hue step along the trail is what makes one ball paint a
            // rainbow streak rather than four samples of the same colour.
            const RGB8 c = ColorUtils::hsv2rgb((uint8_t)(pbHue[i] - k * 24), 255, v);
            Draw::px(buf, pbTrX[i][k], pbTrY[i][k], c.r, c.g, c.b);
        }
    }
    for (uint8_t i = 0; i < pbLive; i++) {
        // Desaturated so the ball reads as the brightest point on the panel
        // whatever hue it is currently on: a fully saturated blue ball at v=255
        // is dimmer to the eye than a saturated yellow trail sample at v=160.
        const RGB8 c = ColorUtils::hsv2rgb(pbHue[i], 150, 255);
        Draw::px(buf, (int16_t)(pbX[i] / 16), (int16_t)(pbY[i] / 16), c.r, c.g, c.b);
    }

    // Score flash: the conceding wall lights in the scorer's hue.
    if (pgFlash) {
        const RGB8 c = ColorUtils::hsv2rgb(pgFlashHue, 255, (uint8_t)(pgFlash * 40));
        for (uint8_t y = 0; y < 16; y++) {
            Draw::px(buf, 0,  y, c.r, c.g, c.b);
            Draw::px(buf, 15, y, c.r, c.g, c.b);
        }
    }
}

// ══ BREAKOUT ══════════════════════════════════════════════════════════════════
//
// One AI paddle clearing a multicoloured brick field, level after level. Brick
// hue runs from the row *and* the column, so a wall is a gradient rather than
// five flat bands, and the whole palette rotates a sixth of the wheel each level.
//
// Four field patterns cycle: solid, checker, pyramid, and a two-hit fortress
// whose top rows have to be struck twice. The ball speeds up a little each
// level, the paddle's aim error stays constant, and so the field eventually
// beats the paddle — at which point the level reloads and it starts again.

static const int16_t BK_MIN_X = 0;
static const int16_t BK_MAX_X = 15 * 16;
static const int16_t BK_PAD_Y = 15;
static const int16_t BK_PAD_SPEED = 13;

void GameEngine::initBreakout() {
    bkLevel  = 1;
    bkMisses = 0;
    bkHue    = rndN(255);
    bkPadX   = (int16_t)(6 * 16);
    bkPadErr = (int8_t)(((int8_t)rndN(11) - 5) * 4);
    bkFlash  = 0;
    bkIdle   = 0;
    bkIdleServes = 0;
    breakoutLoadLevel();
}

void GameEngine::breakoutLoadLevel() {
    const uint8_t pattern = (uint8_t)((bkLevel - 1) % 4);
    for (uint8_t r = 0; r < BK_ROWS; r++)
        for (uint8_t c = 0; c < BK_COLS; c++) {
            uint8_t hp = 1;
            switch (pattern) {
                case 0: hp = 1; break;                                  // solid
                case 1: hp = ((r + c) & 1) ? 1 : 0; break;              // checker
                case 2: hp = (c >= r && c < BK_COLS - r) ? 1 : 0; break; // pyramid
                default: hp = (r < 2) ? 2 : 1; break;                   // fortress
            }
            bkBrick[r][c] = hp;
        }
    bkHue = (uint8_t)(bkHue + 43);
    breakoutServe();
}

void GameEngine::breakoutServe() {
    bkX  = (int16_t)((2 + rndN(12)) * 16);
    bkY  = (int16_t)(11 * 16);
    bkVX = (int16_t)((rndN(2) ? 1 : -1) * (8 + (int16_t)rndN(5) + (int16_t)bkLevel));
    bkVY = (int16_t)(-(10 + (int16_t)rndN(3) + (int16_t)bkLevel));
    bkBallHue = rndN(255);
    bkServeDelay = 6;
    bkIdle = 0;
    for (uint8_t t = 0; t < BK_TRAIL; t++) {
        bkTrX[t] = (int8_t)(bkX / 16);
        bkTrY[t] = (int8_t)(bkY / 16);
    }
}

uint8_t GameEngine::breakoutBricksLeft() const {
    uint8_t n = 0;
    for (uint8_t r = 0; r < BK_ROWS; r++)
        for (uint8_t c = 0; c < BK_COLS; c++)
            if (bkBrick[r][c]) n++;
    return n;
}

void GameEngine::stepBreakout() {
    if (bkFlash) bkFlash--;
    bkBallHue = (uint8_t)(bkBallHue + 5);

    // ── Paddle ────────────────────────────────────────────────────────────────
    // Straight-line prediction of where the ball crosses the paddle row, with a
    // constant aim error. No wall-bounce modelling: the misses that error causes
    // are the only reason the field ever wins.
    int16_t want = bkX;
    if (bkVY > 0) {
        const int16_t dy = (int16_t)(BK_PAD_Y * 16 - bkY);
        want = (int16_t)(bkX + (int32_t)bkVX * dy / (bkVY ? bkVY : 1));
    }
    want = (int16_t)(want - (BK_PAD_W / 2) * 16 + (int16_t)bkPadErr);
    if (want < 0) want = 0;
    if (want > (int16_t)((16 - BK_PAD_W) * 16)) want = (int16_t)((16 - BK_PAD_W) * 16);
    if (bkPadX < want) bkPadX = (int16_t)(bkPadX + (want - bkPadX > BK_PAD_SPEED ? BK_PAD_SPEED : want - bkPadX));
    else if (bkPadX > want) bkPadX = (int16_t)(bkPadX - (bkPadX - want > BK_PAD_SPEED ? BK_PAD_SPEED : bkPadX - want));

    if (bkServeDelay) { bkServeDelay--; return; }

    // ── Anti-stall ────────────────────────────────────────────────────────────
    // A deterministic paddle plus a deterministic ball settles into a limit
    // cycle: with one brick left in a top corner the ball orbited the right of
    // the field for 27 simulated minutes without ever reaching it. Re-randomising
    // the paddle's aim on every return (below) breaks most of those cycles, and
    // this timer catches the rest — first by re-serving, then by conceding the
    // field so the level can never be held hostage by one unreachable brick.
    if (++bkIdle > BK_IDLE_LIMIT) {
        bkIdle = 0;
        if (++bkIdleServes >= 2) {
            bkLevel++;
            bkMisses = 0;
            bkIdleServes = 0;
            bkFlash = 6;
            breakoutLoadLevel();
        } else {
            breakoutServe();
        }
        return;
    }

    // ── Ball ──────────────────────────────────────────────────────────────────
    for (uint8_t t = BK_TRAIL - 1; t > 0; t--) { bkTrX[t] = bkTrX[t - 1]; bkTrY[t] = bkTrY[t - 1]; }
    bkTrX[0] = (int8_t)(bkX / 16);
    bkTrY[0] = (int8_t)(bkY / 16);

    const int8_t prevCy = (int8_t)(bkY / 16);

    bkX = (int16_t)(bkX + bkVX);
    bkY = (int16_t)(bkY + bkVY);

    if (bkX < BK_MIN_X)      { bkX = (int16_t)-bkX;                    bkVX = (int16_t)-bkVX; }
    if (bkX > BK_MAX_X)      { bkX = (int16_t)(2 * BK_MAX_X - bkX);    bkVX = (int16_t)-bkVX; }
    if (bkY < 0)             { bkY = (int16_t)-bkY;                    bkVY = (int16_t)-bkVY; }

    // ── Bricks ────────────────────────────────────────────────────────────────
    const int8_t cx = (int8_t)(bkX / 16);
    const int8_t cy = (int8_t)(bkY / 16);
    if (cy >= (int8_t)BK_TOP && cy < (int8_t)(BK_TOP + BK_ROWS) && cx >= 0 && cx < 16) {
        const uint8_t r = (uint8_t)(cy - BK_TOP);
        const uint8_t c = (uint8_t)(cx / 2);
        if (bkBrick[r][c]) {
            bkBrick[r][c]--;
            // Reflect on the axis the ball actually entered from: a brick struck
            // from the side must not send the ball back the way it came.
            if (prevCy != cy) bkVY = (int16_t)-bkVY;
            else              bkVX = (int16_t)-bkVX;
            bkY = (int16_t)(bkY + (prevCy != cy ? (bkVY > 0 ? 16 : -16) : 0));
            bkFlash = 2;
            bkIdle  = 0;
            bkIdleServes = 0;

            if (breakoutBricksLeft() == 0) {
                bkLevel++;
                bkMisses = 0;
                breakoutLoadLevel();
                return;
            }
        }
    }

    // ── Paddle / floor ────────────────────────────────────────────────────────
    if (bkY >= BK_PAD_Y * 16 && bkVY > 0) {
        const int16_t rel = (int16_t)(bkX - bkPadX);
        if (rel >= -8 && rel <= (int16_t)(BK_PAD_W * 16)) {
            bkY  = (int16_t)(BK_PAD_Y * 16);
            bkVY = (int16_t)-bkVY;
            bkVX = (int16_t)(bkVX + (rel - (BK_PAD_W * 16) / 2) / 5);
            if (bkVX > 16)  bkVX = 16;
            if (bkVX < -16) bkVX = -16;
            bkPadErr = (int8_t)(((int8_t)rndN(11) - 5) * 4);
        } else if (bkY > 15 * 16) {
            bkMisses++;
            bkFlash  = 6;
            bkPadErr = (int8_t)(((int8_t)rndN(11) - 5) * 4);
            if (bkMisses >= 3) { bkMisses = 0; breakoutLoadLevel(); }
            else               breakoutServe();
            return;
        }
    }

    // A near-vertical ball turns the game into a metronome on one column.
    if (bkVX > -3 && bkVX < 3) bkVX = (int16_t)(bkVX >= 0 ? 4 : -4);
}

void GameEngine::drawBreakout() {
    Draw::clear(buf);

    // Bricks: hue walks with both row and column so the wall is a gradient, and
    // a two-hit brick is drawn at full value while a cracked one is halved.
    for (uint8_t r = 0; r < BK_ROWS; r++)
        for (uint8_t c = 0; c < BK_COLS; c++) {
            if (!bkBrick[r][c]) continue;
            const uint8_t v = (uint8_t)(bkBrick[r][c] > 1 ? 255 : 170);
            const RGB8 col = ColorUtils::hsv2rgb((uint8_t)(bkHue + r * 26 + c * 7), 235, v);
            Draw::px(buf, (int16_t)(c * 2),     (int16_t)(BK_TOP + r), col.r, col.g, col.b);
            Draw::px(buf, (int16_t)(c * 2 + 1), (int16_t)(BK_TOP + r), col.r, col.g, col.b);
        }

    // Ball trail, oldest first.
    for (uint8_t t = BK_TRAIL; t > 0; t--) {
        const uint8_t k = (uint8_t)(t - 1);
        const uint8_t v = (uint8_t)(140 - k * 32);
        const RGB8 c = ColorUtils::hsv2rgb((uint8_t)(bkBallHue - k * 12), 255, v);
        Draw::px(buf, bkTrX[k], bkTrY[k], c.r, c.g, c.b);
    }

    const RGB8 ball = ColorUtils::hsv2rgb(bkBallHue, 200, 255);
    Draw::px(buf, (int16_t)(bkX / 16), (int16_t)(bkY / 16), ball.r, ball.g, ball.b);

    // Paddle, brightest in the middle so its centre is easy to track.
    for (uint8_t k = 0; k < BK_PAD_W; k++) {
        const bool mid = (k == 1 || k == 2);
        Draw::px(buf, (int16_t)(bkPadX / 16 + k), BK_PAD_Y,
                 mid ? 220 : 140, mid ? 240 : 160, 255);
    }

    if (bkFlash) {
        const uint8_t v = (uint8_t)(bkFlash * 20);
        Draw::rect(buf, 0, 0, 16, 1, v, v, (uint8_t)(v / 2));
    }
}
