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

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void GameEngine::begin(Game g, uint32_t nowMs, uint32_t seed) {
    cur      = (g >= Game::COUNT) ? Game::SNAKE : g;
    rngState = seed ? seed : 0x1234567u;   // xorshift32 must never be seeded 0
    reset(nowMs);
}

void GameEngine::reset(uint32_t nowMs) {
    lastStepMs = nowMs;
    steps      = 0;
    Draw::clear(buf);

    switch (cur) {
        case Game::SNAKE:    initSnake();    drawSnake();    break;
        case Game::LIFE:     initLife();     drawLife();     break;
        case Game::INVADERS: initInvaders(); drawInvaders(); break;
        case Game::DINO:     initDino();     drawDino();     break;
        default: break;
    }
}

void GameEngine::update(uint32_t nowMs) {
    uint16_t interval;
    switch (cur) {
        case Game::SNAKE:    interval = SNAKE_STEP_MS;    break;
        case Game::LIFE:     interval = LIFE_STEP_MS;     break;
        case Game::INVADERS: interval = INVADERS_STEP_MS; break;
        case Game::DINO:     interval = DINO_STEP_MS;     break;
        default:             interval = 200;              break;
    }

    if ((uint32_t)(nowMs - lastStepMs) < interval) return;
    lastStepMs = nowMs;
    steps++;

    switch (cur) {
        case Game::SNAKE:    stepSnake();    drawSnake();    break;
        case Game::LIFE:     stepLife();     drawLife();     break;
        case Game::INVADERS: stepInvaders(); drawInvaders(); break;
        case Game::DINO:     stepDino();     drawDino();     break;
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

// ══ LIFE ══════════════════════════════════════════════════════════════════════
//
// Conway on a 32x32 torus viewed through a 16x16 window. A decayed heatmap of
// cell changes drives a camera that drifts toward wherever the most is
// happening, so the view never sits on a field of static blocks.

uint8_t GameEngine::wrapWorld(int16_t v) {
    while (v < 0)                     v += LIFE_WORLD;
    while (v >= (int16_t)LIFE_WORLD)  v -= LIFE_WORLD;
    return (uint8_t)v;
}

void GameEngine::initLife() {
    memset(lAlive, 0, sizeof(lAlive));
    memset(lAge,   0, sizeof(lAge));
    memset(lAct,   0, sizeof(lAct));

    for (uint16_t i = 0; i < LIFE_WORLD * LIFE_WORLD; i++) {
        if (rndN(100) < 28) { lAlive[i] = 1; lAge[i] = 1; }
    }

    lGen  = 0;
    camX  = (int16_t)(((LIFE_WORLD - 16) / 2) * 16);   // /16 fixed point
    camY  = camX;
    camTX = camX;
    camTY = camY;
}

uint16_t GameEngine::lifePopulation() const {
    uint16_t pop = 0;
    for (uint16_t i = 0; i < LIFE_WORLD * LIFE_WORLD; i++) pop = (uint16_t)(pop + lAlive[i]);
    return pop;
}

uint16_t GameEngine::windowActivity(uint8_t x0, uint8_t y0) const {
    uint16_t sum = 0;
    for (uint8_t dy = 0; dy < 16; dy++) {
        const uint8_t wy = wrapWorld((int16_t)(y0 + dy));
        for (uint8_t dx = 0; dx < 16; dx++) {
            const uint8_t wx = wrapWorld((int16_t)(x0 + dx));
            sum = (uint16_t)(sum + lAct[wy * LIFE_WORLD + wx]);
        }
    }
    return sum;
}

void GameEngine::stepLife() {
    uint16_t pop = 0;

    for (uint8_t y = 0; y < LIFE_WORLD; y++) {
        for (uint8_t x = 0; x < LIFE_WORLD; x++) {
            uint8_t n = 0;
            for (int8_t dy = -1; dy <= 1; dy++)
                for (int8_t dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    n = (uint8_t)(n + lAlive[wrapWorld((int16_t)y + dy) * LIFE_WORLD +
                                            wrapWorld((int16_t)x + dx)]);
                }

            const uint16_t i    = (uint16_t)(y * LIFE_WORLD + x);
            const bool     was  = lAlive[i] != 0;
            const bool     will = was ? (n == 2 || n == 3) : (n == 3);

            lNext[i]    = will ? 1 : 0;
            lNextAge[i] = 0;
            if (will) {
                pop++;
                lNextAge[i] = was ? (uint8_t)(lAge[i] < 7 ? lAge[i] + 1 : 7) : 1;
            }

            // Decay the heatmap toward zero, spiking where a cell changed state.
            uint8_t a = (uint8_t)((uint16_t)lAct[i] * 200 / 256);
            if (will != was) a = (uint8_t)(a + 60 > 255 ? 255 : a + 60);
            lAct[i] = a;
        }
    }

    memcpy(lAlive, lNext,    sizeof(lAlive));
    memcpy(lAge,   lNextAge, sizeof(lAge));
    lGen++;

    // A dead or near-dead world is visually boring: reseed rather than idle.
    if (pop < 20) { initLife(); return; }

    if (lGen % 8 == 0) {
        uint16_t best = 0;
        uint8_t  bx = 0, by = 0;
        for (uint8_t y0 = 0; y0 < LIFE_WORLD; y0 = (uint8_t)(y0 + 4))
            for (uint8_t x0 = 0; x0 < LIFE_WORLD; x0 = (uint8_t)(x0 + 4)) {
                const uint16_t s = windowActivity(x0, y0);
                if (s > best) { best = s; bx = x0; by = y0; }
            }
        camTX = (int16_t)(bx * 16);
        camTY = (int16_t)(by * 16);
    }

    // Ease toward the target along the shorter way round the torus.
    const int16_t span = (int16_t)(LIFE_WORLD * 16);
    int16_t dx = (int16_t)(camTX - camX);
    int16_t dy = (int16_t)(camTY - camY);
    if (dx >  span / 2) dx = (int16_t)(dx - span);
    if (dx < -span / 2) dx = (int16_t)(dx + span);
    if (dy >  span / 2) dy = (int16_t)(dy - span);
    if (dy < -span / 2) dy = (int16_t)(dy + span);

    camX = (int16_t)(camX + dx / 8);
    camY = (int16_t)(camY + dy / 8);
    while (camX < 0)     camX = (int16_t)(camX + span);
    while (camX >= span) camX = (int16_t)(camX - span);
    while (camY < 0)     camY = (int16_t)(camY + span);
    while (camY >= span) camY = (int16_t)(camY - span);
}

void GameEngine::drawLife() {
    Draw::clear(buf);

    // Age ramp: fresh cells pale green, survivors deepening toward blue.
    static const uint8_t PAL[8][3] = {
        {202, 255, 191}, {158, 240,  26}, {112, 224,   0}, { 56, 176,   0},
        {  0, 128,   0}, {  3, 102,  43}, { 11,  79, 108}, {  3,  57, 100},
    };

    const uint8_t cx = (uint8_t)(camX / 16);
    const uint8_t cy = (uint8_t)(camY / 16);

    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i = (uint16_t)(wrapWorld((int16_t)(cy + y)) * LIFE_WORLD +
                                          wrapWorld((int16_t)(cx + x)));
            if (!lAlive[i]) continue;
            const uint8_t a = (uint8_t)(lAge[i] > 7 ? 7 : lAge[i]);
            Draw::px(buf, x, y, PAL[a][0], PAL[a][1], PAL[a][2]);
        }
}

// ══ INVADERS ══════════════════════════════════════════════════════════════════
//
// A 5x3 formation marches sideways, drops a row at each wall, and is picked off
// by a cannon that slides under the lowest surviving column and fires on
// alignment. Clearing the field or letting the formation reach the cannon both
// start a fresh wave, so play never halts.

void GameEngine::initInvaders() {
    invMask  = (uint16_t)((1u << (INV_COLS * INV_ROWS)) - 1u);
    invX     = 3;
    invY     = 0;
    invDir   = 1;
    invFrame = 0;
    invTick  = 0;
    bulX = bulY = -1;
    canX = 7;
    expX = expY = -1;
    expTtl = 0;
}

void GameEngine::invaderPos(uint8_t i, int8_t& x, int8_t& y) const {
    x = (int8_t)(invX + (i % INV_COLS) * 3);
    y = (int8_t)(invY + (i / INV_COLS) * 3);
}

uint8_t GameEngine::invadersAlive() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < INV_COLS * INV_ROWS; i++) if (invMask & (1u << i)) n++;
    return n;
}

void GameEngine::stepInvaders() {
    if (invMask == 0) { wave++; initInvaders(); return; }

    // ── March, reversing and dropping when a live invader reaches an edge ─────
    // The formation advances only every INV_MOVE_EVERY steps while bullets move
    // every step, so a shot fired on alignment still connects when it arrives.
    if (++invTick >= INV_MOVE_EVERY) {
        invTick  = 0;
        invFrame ^= 1;

        bool bounce = false;
        for (uint8_t i = 0; i < INV_COLS * INV_ROWS; i++) {
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

        // Formation reached the cannon — start over.
        if (invY + (INV_ROWS - 1) * 3 + 1 >= 14) { wave++; initInvaders(); return; }
    }

    // ── Cannon tracks the lowest surviving invader ────────────────────────────
    int8_t targetX = canX, lowest = -1;
    for (uint8_t i = 0; i < INV_COLS * INV_ROWS; i++) {
        if (!(invMask & (1u << i))) continue;
        int8_t x, y;
        invaderPos(i, x, y);
        if (y > lowest) { lowest = y; targetX = x; }
    }
    if (canX < targetX) canX++;
    else if (canX > targetX) canX--;
    if (canX < 1)  canX = 1;
    if (canX > 14) canX = 14;

    // ── Bullet ────────────────────────────────────────────────────────────────
    if (bulY >= 0) {
        bulY = (int8_t)(bulY - 2);
        if (bulY < 0) { bulX = bulY = -1; }
        else {
            for (uint8_t i = 0; i < INV_COLS * INV_ROWS; i++) {
                if (!(invMask & (1u << i))) continue;
                int8_t x, y;
                invaderPos(i, x, y);
                // Hitbox is widened by a pixel each side: the formation still
                // drifts slightly during the bullet's flight, and a screensaver
                // that visibly never hits anything looks broken.
                if (bulX >= x - 1 && bulX <= x + 2 && bulY >= y && bulY <= y + 1) {
                    invMask = (uint16_t)(invMask & ~(1u << i));
                    expX = x; expY = y; expTtl = 3;
                    bulX = bulY = -1;
                    break;
                }
            }
        }
    } else if (canX == targetX) {
        bulX = canX;
        bulY = 13;
    }

    if (expTtl) expTtl--;
    else        { expX = expY = -1; }
}

void GameEngine::drawInvaders() {
    Draw::clear(buf);

    // Deep space backdrop keeps the panel from looking switched off.
    Draw::fill(buf, 0, 0, 6);

    // Row-coloured invaders, wiggling one pixel each step.
    static const uint8_t ROWC[3][3] = {
        {255,  90,  90}, {255, 190,  70}, {150, 220, 255},
    };

    for (uint8_t i = 0; i < INV_COLS * INV_ROWS; i++) {
        if (!(invMask & (1u << i))) continue;
        int8_t x, y;
        invaderPos(i, x, y);
        const uint8_t* c = ROWC[i / INV_COLS];

        Draw::px(buf, x,     y,     c[0], c[1], c[2]);
        Draw::px(buf, x + 1, y,     c[0], c[1], c[2]);
        // Alternating "legs" read as movement at this scale.
        if (invFrame) Draw::px(buf, x,     y + 1, c[0], c[1], c[2]);
        else          Draw::px(buf, x + 1, y + 1, c[0], c[1], c[2]);
    }

    if (expTtl && expX >= 0) {
        Draw::px(buf, expX,     expY,     255, 255, 200);
        Draw::px(buf, expX + 1, expY,     255, 200, 100);
        Draw::px(buf, expX,     expY + 1, 255, 200, 100);
        Draw::px(buf, expX + 1, expY + 1, 255, 255, 200);
    }

    if (bulY >= 0) Draw::px(buf, bulX, bulY, 255, 255, 255);

    // Cannon: a three-pixel base with a muzzle.
    Draw::px(buf, canX,     15, 90, 255, 140);
    Draw::px(buf, canX - 1, 15, 60, 200, 110);
    Draw::px(buf, canX + 1, 15, 60, 200, 110);
    Draw::px(buf, canX,     14, 160, 255, 190);
}

// ══ DINO ══════════════════════════════════════════════════════════════════════
//
// Endless runner. The jump is triggered by lookahead rather than reaction, so
// the dino clears every obstacle — a screensaver should not visibly fail.
// Day flips to night periodically, inverting the palette.

void GameEngine::initDino() {
    dinoY   = 0;
    dinoVY  = 0;
    for (uint8_t i = 0; i < OBS_MAX; i++) { obsX[i] = -1; obsH[i] = 0; }
    dinoSpeed = 10;
    dinoDist  = 0;
    night     = false;
}

void GameEngine::stepDino() {
    dinoDist++;

    // Speed ramps then resets, so the pace visibly builds and restarts.
    dinoSpeed = (uint16_t)(10 + (dinoDist % 900) / 60);
    if (dinoDist % 340 == 0) night = !night;

    // ── Obstacles ─────────────────────────────────────────────────────────────
    // obsX < 0 is the single source of truth for "slot free". An earlier version
    // also kept an obsUsed counter, which leaked: a slot at -10 was free for
    // spawning but had not yet crossed the despawn threshold, so it was recycled
    // without ever being decremented and spawning stopped once it reached OBS_MAX.
    uint8_t live = 0;
    int16_t rightmost = -1000;

    for (uint8_t i = 0; i < OBS_MAX; i++) {
        if (obsX[i] < 0) continue;
        obsX[i] = (int16_t)(obsX[i] - dinoSpeed);
        if (obsX[i] < -20) { obsX[i] = -1; obsH[i] = 0; continue; }
        live++;
        if (obsX[i] > rightmost) rightmost = obsX[i];
    }

    // Spawn only once the last obstacle has left room, so cacti stay clearable.
    if (live < OBS_MAX && rightmost < 90 && rndN(100) < 22) {
        for (uint8_t i = 0; i < OBS_MAX; i++) {
            if (obsX[i] >= 0) continue;
            obsX[i] = 160;                       // just off the right edge
            obsH[i] = (uint8_t)(2 + rndN(3));    // 2-4 px tall
            break;
        }
    }

    // ── Auto-jump ─────────────────────────────────────────────────────────────
    // Take off while an obstacle sits in a speed-relative window ahead of the
    // dino, so the arc peaks over the cactus at any scroll speed. Reacting on
    // contact would be too late once dinoSpeed ramps up.
    if (dinoY == 0 && dinoVY == 0) {
        for (uint8_t i = 0; i < OBS_MAX; i++) {
            if (obsX[i] < 0) continue;
            const int16_t gap = (int16_t)(obsX[i] - 45);   // dino front edge at x=4
            if (gap > (int16_t)(2 * dinoSpeed) && gap <= (int16_t)(5 * dinoSpeed)) {
                dinoVY = JUMP_V0;
                break;
            }
        }
    }

    if (dinoVY != 0 || dinoY > 0) {
        dinoY  = (int16_t)(dinoY + dinoVY);
        dinoVY = (int16_t)(dinoVY - JUMP_GRAV);
        if (dinoY <= 0) { dinoY = 0; dinoVY = 0; }
    }
}

void GameEngine::drawDino() {
    // Palette flips wholesale between day and night.
    const uint8_t bgR = night ? 4   : 20,  bgG = night ? 6   : 24, bgB = night ? 18 : 30;
    const uint8_t fgR = night ? 210 : 60,  fgG = night ? 220 : 70, fgB = night ? 255 : 80;
    const uint8_t grR = night ? 60  : 110, grG = night ? 70  : 100, grB = night ? 110 : 70;
    const uint8_t caR = night ? 90  : 60,  caG = night ? 200 : 170, caB = night ? 150 : 70;

    Draw::fill(buf, bgR, bgG, bgB);

    // Ground line plus a scrolling speckle so motion is visible even with no
    // obstacle on screen.
    Draw::rect(buf, 0, 14, 16, 1, grR, grG, grB);
    for (uint8_t x = 0; x < 16; x++)
        if (((x + (dinoDist / 2)) % 5) == 0)
            Draw::px(buf, x, 15, (uint8_t)(grR / 2), (uint8_t)(grG / 2), (uint8_t)(grB / 2));

    // Obstacles: cacti standing on the ground line.
    for (uint8_t i = 0; i < OBS_MAX; i++) {
        if (obsX[i] < 0) continue;
        const int16_t px = (int16_t)(obsX[i] / 10);
        if (px < -2 || px > 17) continue;
        for (uint8_t h = 0; h < obsH[i]; h++)
            Draw::px(buf, px, (int16_t)(13 - h), caR, caG, caB);
        // A single arm makes it read as a cactus rather than a bar.
        if (obsH[i] >= 3) Draw::px(buf, (int16_t)(px + 1), 12, caR, caG, caB);
    }

    // Dino: 3x4 body whose legs alternate while grounded.
    const int16_t by = (int16_t)(13 - dinoY / 10);
    Draw::px(buf, 3, (int16_t)(by - 3), fgR, fgG, fgB);
    Draw::px(buf, 4, (int16_t)(by - 3), fgR, fgG, fgB);
    Draw::px(buf, 2, (int16_t)(by - 2), fgR, fgG, fgB);
    Draw::px(buf, 3, (int16_t)(by - 2), fgR, fgG, fgB);
    Draw::px(buf, 4, (int16_t)(by - 2), fgR, fgG, fgB);
    Draw::px(buf, 3, (int16_t)(by - 1), fgR, fgG, fgB);
    Draw::px(buf, 4, (int16_t)(by - 1), fgR, fgG, fgB);

    if (dinoY == 0 && (dinoDist / 3) % 2) {
        Draw::px(buf, 2, by, fgR, fgG, fgB);
        Draw::px(buf, 4, by, fgR, fgG, fgB);
    } else {
        Draw::px(buf, 3, by, fgR, fgG, fgB);
        Draw::px(buf, 4, by, fgR, fgG, fgB);
    }
}
