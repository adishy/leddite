#include "GameEngine.h"
#include "Draw.h"
#include "test_harness.h"
#include <string.h>

// Games are screensavers: they must never end, never stall and never go blank.
// These are soak tests over tens of thousands of steps because both real bugs
// found during development (see docs/adr/0009) only surfaced after the opening
// few seconds of play.

static GameEngine ge;

static uint16_t litPixels(const uint8_t* b) {
    uint16_t n = 0;
    for (uint16_t i = 0; i < 256; i++)
        if (b[i * 3] | b[i * 3 + 1] | b[i * 3 + 2]) n++;
    return n;
}

static bool anyLit(const uint8_t* b) { return litPixels(b) > 0; }

static uint16_t stepMs(Game g) {
    switch (g) {
        case Game::SNAKE:    return GameEngine::SNAKE_STEP_MS;
        case Game::LIFE:     return GameEngine::LIFE_STEP_MS;
        case Game::INVADERS: return GameEngine::INVADERS_STEP_MS;
        default:             return GameEngine::DINO_STEP_MS;
    }
}

// ── Cross-game ────────────────────────────────────────────────────────────────

void test_determinism() {
    TEST("same seed reproduces identical frames");
    for (uint8_t g = 0; g < (uint8_t)Game::COUNT; g++) {
        uint8_t snapshot[Draw::SIZE];
        const uint16_t iv = stepMs((Game)g);

        ge.begin((Game)g, 0, 0xABCDEF);
        uint32_t t = 0;
        for (int i = 0; i < 500; i++) { t += iv; ge.update(t); }
        memcpy(snapshot, ge.buffer(), Draw::SIZE);

        ge.begin((Game)g, 0, 0xABCDEF);
        t = 0;
        for (int i = 0; i < 500; i++) { t += iv; ge.update(t); }

        ASSERT(memcmp(snapshot, ge.buffer(), Draw::SIZE) == 0,
               "replay with the same seed diverged");
    }
    PASS();
}

void test_no_blank_frames() {
    TEST("no game ever renders an empty panel");
    for (uint8_t g = 0; g < (uint8_t)Game::COUNT; g++) {
        const uint16_t iv = stepMs((Game)g);
        ge.begin((Game)g, 0, 0x5EED + g);
        uint32_t t = 0;
        for (int i = 0; i < 4000; i++) {
            t += iv;
            ge.update(t);
            if (!anyLit(ge.buffer())) { ASSERT(false, "blank frame rendered"); return; }
        }
    }
    PASS();
}

void test_update_respects_step_interval() {
    TEST("update() is rate-limited, not per-call");
    ge.begin(Game::SNAKE, 0, 1);
    const uint32_t before = ge.stepCount();
    for (int i = 0; i < 50; i++) ge.update(10);      // far below SNAKE_STEP_MS
    ASSERT_EQ(ge.stepCount(), before, "stepped despite no time passing");
    ge.update(GameEngine::SNAKE_STEP_MS + 1);
    ASSERT_EQ(ge.stepCount(), before + 1, "did not step once the interval elapsed");
    PASS();
}

// ── Snake ─────────────────────────────────────────────────────────────────────

void test_snake_never_self_overlaps() {
    TEST("snake body never occupies a cell twice");
    // Lit pixels must equal body length + 1 for the food. Any overlap shows up
    // as a shortfall, and food is never placed on the body.
    for (uint32_t seed = 1; seed <= 25; seed++) {
        ge.begin(Game::SNAKE, 0, seed * 2654435761u);
        uint32_t t = 0;
        for (int i = 0; i < 2000; i++) {
            t += GameEngine::SNAKE_STEP_MS;
            ge.update(t);
            if (litPixels(ge.buffer()) != (uint16_t)(ge.snakeLength() + 1)) {
                ASSERT(false, "snake overlapped itself or its food");
                return;
            }
        }
    }
    PASS();
}

void test_snake_grows_and_survives() {
    TEST("snake eats, grows, and never gets stuck");
    ge.begin(Game::SNAKE, 0, 0xC0FFEE);
    uint32_t t = 0;
    uint16_t maxLen = 0;
    for (int i = 0; i < 20000; i++) {
        t += GameEngine::SNAKE_STEP_MS;
        ge.update(t);
        if (ge.snakeLength() > maxLen) maxLen = ge.snakeLength();
        ASSERT(ge.snakeLength() >= 3, "snake shrank below its start length");
        ASSERT(ge.snakeLength() <= GameEngine::SNAKE_MAX, "snake exceeded its cap");
    }
    ASSERT(maxLen > 10, "snake never meaningfully grew");
    PASS();
}

// ── Life ──────────────────────────────────────────────────────────────────────

void test_life_never_dies_out() {
    TEST("life reseeds rather than going extinct");
    for (uint32_t seed = 1; seed <= 5; seed++) {
        ge.begin(Game::LIFE, 0, seed * 40503u + 7);
        uint32_t t = 0;
        for (int i = 0; i < 3000; i++) {
            t += GameEngine::LIFE_STEP_MS;
            ge.update(t);
            if (ge.lifePopulation() == 0) { ASSERT(false, "population hit zero"); return; }
        }
    }
    PASS();
}

// ── Invaders ──────────────────────────────────────────────────────────────────

void test_invaders_waves_restart() {
    TEST("invaders keep starting fresh waves");
    // Regression: with 5 columns the formation spanned 13px of a 16px field,
    // leaving 3px of travel. It bounced every ~3 steps and reached the bottom
    // before the cannon could ever land a shot — waves never restarted.
    ge.begin(Game::INVADERS, 0, 0x1234);
    uint32_t t = 0;
    int waves = 0;
    uint8_t prev = ge.invadersAlive();
    for (int i = 0; i < 20000; i++) {
        t += GameEngine::INVADERS_STEP_MS;
        ge.update(t);
        const uint8_t a = ge.invadersAlive();
        ASSERT(a <= 12, "invader count exceeded the formation size");
        if (a > prev) waves++;
        prev = a;
    }
    ASSERT(waves > 5, "no wave ever restarted");
    PASS();
}

void test_invaders_cannon_scores_kills() {
    TEST("cannon actually destroys invaders");
    ge.begin(Game::INVADERS, 0, 0x99);
    uint32_t t = 0;
    int kills = 0;
    uint8_t prev = ge.invadersAlive();
    for (int i = 0; i < 3000; i++) {
        t += GameEngine::INVADERS_STEP_MS;
        ge.update(t);
        const uint8_t a = ge.invadersAlive();
        if (a < prev) kills += (prev - a);
        prev = a;
    }
    ASSERT(kills > 20, "cannon landed almost no hits");
    PASS();
}

// ── Dino ──────────────────────────────────────────────────────────────────────

void test_dino_jumps_and_lands() {
    TEST("dino jumps, lands, and keeps jumping");
    // Regression: obsUsed leaked — a slot at -10 was free to respawn but had not
    // crossed the -30 despawn threshold, so it was recycled without being
    // decremented. The counter saturated at OBS_MAX and every obstacle stopped
    // spawning after ~34 steps, leaving the dino grounded forever.
    ge.begin(Game::DINO, 0, 0x777);
    uint32_t t = 0;
    int airborne = 0, grounded = 0, lateJumps = 0;
    for (int i = 0; i < 30000; i++) {
        t += GameEngine::DINO_STEP_MS;
        ge.update(t);
        if (ge.dinoGrounded()) grounded++;
        else { airborne++; if (i > 20000) lateJumps++; }
    }
    ASSERT(airborne > 100, "dino never jumped");
    ASSERT(grounded > 100, "dino never landed");
    ASSERT(lateJumps > 50, "dino stopped jumping late in the run (spawn starvation)");
    PASS();
}

void test_dino_stays_on_screen() {
    TEST("dino never jumps off the top of the panel");
    // The first tuning peaked at 16.8px on a 16px panel.
    ge.begin(Game::DINO, 0, 0x31337);
    uint32_t t = 0;
    for (int i = 0; i < 20000; i++) {
        t += GameEngine::DINO_STEP_MS;
        ge.update(t);
        // Row 0 clear of the body means the sprite never left the panel; the
        // background fill means we check for the bright body colour instead.
        const uint8_t* b = ge.buffer();
        for (uint8_t x = 2; x <= 4; x++) {
            const uint16_t i0 = (uint16_t)(0 * 16 + x) * 3;
            ASSERT(!(b[i0] > 180 && b[i0 + 1] > 180),
                   "dino sprite reached the top row");
        }
    }
    PASS();
}

int main() {
    printf("=== GameEngine Unit Tests ===\n");
    test_determinism();
    test_no_blank_frames();
    test_update_respects_step_interval();
    test_snake_never_self_overlaps();
    test_snake_grows_and_survives();
    test_life_never_dies_out();
    test_invaders_waves_restart();
    test_invaders_cannon_scores_kills();
    test_dino_jumps_and_lands();
    test_dino_stays_on_screen();
    SUMMARY("GameEngine");
}
