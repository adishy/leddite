#include "GameEngine.h"
#include "Draw.h"
#include "test_harness.h"
#include <string.h>

// Games are screensavers: they must never end, never stall and never go blank.
// These are soak tests over tens of thousands of steps because every real bug
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

static uint16_t stepMs(Game g) { return GameEngine::stepIntervalMs(g); }

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

void test_every_game_index_is_reachable() {
    TEST("Game::COUNT games all start and run");
    // UiController maps menu row -> Game by index, so a mismatch between the
    // enum and the menu shows up here first.
    for (uint8_t g = 0; g < (uint8_t)Game::COUNT; g++) {
        ge.begin((Game)g, 0, 0x1000 + g);
        ASSERT_EQ((uint8_t)ge.game(), g, "begin() did not select the requested game");
        ASSERT(stepMs((Game)g) > 0, "game has no step interval");
    }
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

// ── Invaders ──────────────────────────────────────────────────────────────────

void test_invaders_campaign_advances() {
    TEST("invaders climbs levels and reaches the last one");
    // Regression: with 5 columns on a 3px pitch the formation spanned 14px of a
    // 16px field, bounced every other step and rained to the bottom before the
    // cannon could land a shot. Levels never completed.
    ge.begin(Game::INVADERS, 0, 0x1234);
    uint32_t t = 0;
    uint8_t maxLevel = 0;
    for (int i = 0; i < 40000; i++) {
        t += GameEngine::INVADERS_STEP_MS;
        ge.update(t);
        if (ge.invadersLevel() > maxLevel) maxLevel = ge.invadersLevel();
        ASSERT(ge.invadersAlive() <= 25, "invader count exceeded the formation size");
        ASSERT(ge.invadersLevel() <= GameEngine::INV_CAMPAIGN, "level ran past the campaign");
    }
    ASSERT_EQ(maxLevel, GameEngine::INV_CAMPAIGN, "campaign never reached its last level");
    ASSERT(ge.invadersRuns() > 20, "campaigns are not restarting");
    PASS();
}

void test_invaders_formation_grows_with_level() {
    TEST("later levels field more invaders than early ones");
    // "More enemies" is a requirement, not a side effect: level 1 must be a
    // smaller formation than the last non-boss level.
    uint8_t firstLevelPeak = 0, lateLevelPeak = 0;
    ge.begin(Game::INVADERS, 0, 0xF00D);
    uint32_t t = 0;
    for (int i = 0; i < 40000; i++) {
        t += GameEngine::INVADERS_STEP_MS;
        ge.update(t);
        const uint8_t a = ge.invadersAlive();
        if (ge.invadersLevel() == 1 && a > firstLevelPeak) firstLevelPeak = a;
        if (ge.invadersLevel() == 5 && a > lateLevelPeak)  lateLevelPeak  = a;
    }
    ASSERT(firstLevelPeak >= 8, "level 1 formation is smaller than expected");
    ASSERT(lateLevelPeak > firstLevelPeak, "the formation never grew with the level");
    PASS();
}

void test_invaders_has_bosses() {
    TEST("a boss appears on every INV_BOSS_EVERY-th level");
    ge.begin(Game::INVADERS, 0, 0xB055);
    uint32_t t = 0;
    int bossSteps = 0;
    bool bossOnBossLevel = false, bossOnPlainLevel = false;
    for (int i = 0; i < 40000; i++) {
        t += GameEngine::INVADERS_STEP_MS;
        ge.update(t);
        if (!ge.invadersBossLevel()) continue;
        bossSteps++;
        if (ge.invadersLevel() % GameEngine::INV_BOSS_EVERY == 0) bossOnBossLevel = true;
        else                                                      bossOnPlainLevel = true;
    }
    ASSERT(bossSteps > 200, "no boss fight ever ran");
    ASSERT(bossOnBossLevel, "boss levels never carried a boss");
    ASSERT(!bossOnPlainLevel, "a boss appeared on a non-boss level");
    PASS();
}

void test_invaders_win_rate_matches_the_dial() {
    TEST("the ship wins winChancePct of its runs");
    // The outcome is rolled once per campaign rather than emerging from the
    // tuning, precisely so this can be asserted. Before that change the measured
    // rate was 0.315 against a 0.30 dial, and it moved whenever a formation size
    // or fire rate was touched.
    uint32_t runs = 0, wins = 0;
    for (uint32_t seed = 1; seed <= 120; seed++) {
        ge.begin(Game::INVADERS, 0, seed * 2654435761u | 1u);
        uint32_t t = 0;
        for (int i = 0; i < 20000; i++) { t += GameEngine::INVADERS_STEP_MS; ge.update(t); }
        runs += ge.invadersRuns();
        wins += ge.invadersWins();
    }
    ASSERT(runs > 2000, "not enough completed runs to measure a rate");
    // 3 sigma on ~10k runs at p=0.3 is under 0.014; 0.03 leaves room for the
    // correlation between runs drawn from one seed's stream.
    const double rate = (double)wins / (double)runs;
    ASSERT(rate > 0.27 && rate < 0.33, "win rate drifted away from INV_WIN_PCT");
    PASS();
}

void test_invaders_win_chance_is_tunable() {
    TEST("setInvadersWinChance moves the rate");
    // The dial has to be a dial: a screensaver whose outcome is fixed at 0.3 by
    // construction is not the same thing as one where 0.3 is the current value.
    ge.setInvadersWinChance(0);
    ge.begin(Game::INVADERS, 0, 0x5151);
    uint32_t t = 0;
    for (int i = 0; i < 20000; i++) { t += GameEngine::INVADERS_STEP_MS; ge.update(t); }
    ASSERT(ge.invadersRuns() > 10, "no runs completed at 0%");
    ASSERT_EQ(ge.invadersWins(), 0u, "the ship won with the dial at 0%");

    ge.setInvadersWinChance(100);
    ge.begin(Game::INVADERS, 0, 0x5151);
    t = 0;
    for (int i = 0; i < 20000; i++) { t += GameEngine::INVADERS_STEP_MS; ge.update(t); }
    ASSERT(ge.invadersRuns() > 5, "no runs completed at 100%");
    ASSERT_EQ(ge.invadersWins(), ge.invadersRuns(), "the ship lost with the dial at 100%");

    ge.setInvadersWinChance(GameEngine::INV_WIN_PCT);
    PASS();
}

// ── Dino ──────────────────────────────────────────────────────────────────────

void test_dino_never_hits_an_obstacle() {
    TEST("the dino clears every cactus it meets");
    // This is the bug the rework exists to fix. The old build had no collision
    // detection at all and a takeoff window derived for a 3px sprite: at the slow
    // end of the speed ramp the dino came down on the cactus and ran through it.
    //
    // Two separate causes had to go. The arc is now planned by simulating it
    // against the real obstacle positions, and the scroll speed only changes on
    // an empty track — a ramp tick mid-approach silently invalidated a plan that
    // was correct when it was made, which was worth about one crash per 22,000
    // steps on its own.
    for (uint32_t seed = 1; seed <= 40; seed++) {
        ge.begin(Game::DINO, 0, seed * 2654435761u | 1u);
        uint32_t t = 0;
        for (int i = 0; i < 20000; i++) { t += GameEngine::DINO_STEP_MS; ge.update(t); }
        if (ge.dinoCrashes() != 0) { ASSERT(false, "the dino hit a cactus"); return; }
    }
    PASS();
}

void test_dino_jumps_and_lands() {
    TEST("dino jumps, lands, and keeps jumping");
    // Regression: obsUsed leaked — a slot at -10 was free to respawn but had not
    // crossed the despawn threshold, so it was recycled without being
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
    // The first tuning peaked at 16.8px on a 16px panel; the current arc peaks at
    // 7.7px, which puts the 6px-tall sprite's crown on row 1.
    ge.begin(Game::DINO, 0, 0x31337);
    uint32_t t = 0;
    for (int i = 0; i < 20000; i++) {
        t += GameEngine::DINO_STEP_MS;
        ge.update(t);
        // The background is black (adr/0012), so "anything lit on row 0" is the
        // whole test — no need to guess a threshold separating sprite from sky.
        const uint8_t* b = ge.buffer();
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i0 = (uint16_t)x * 3;
            ASSERT((b[i0] | b[i0 + 1] | b[i0 + 2]) == 0, "something reached the top row");
        }
    }
    PASS();
}

void test_dino_sprite_reads_as_a_dinosaur() {
    TEST("the runner has a head, a tail and two legs");
    // The old 3x4 blob was the other half of the complaint. These are the three
    // features that make the silhouette read as a T-Rex rather than a brick, so
    // they are asserted rather than left to a screenshot review.
    ge.begin(Game::DINO, 0, 0x0D1);
    uint32_t t = 0;
    // Settle onto the ground with no obstacle in the way.
    for (int i = 0; i < 12; i++) { t += GameEngine::DINO_STEP_MS; ge.update(t); }
    ASSERT(ge.dinoGrounded(), "dino should be grounded this early");

    const uint8_t* b = ge.buffer();
    const uint8_t GROUND_Y = 13;                 // feet row
    const uint8_t TOP = (uint8_t)(GROUND_Y - 5); // 6px tall sprite

    auto lit = [&](int x, int y) {
        const uint16_t i = (uint16_t)((y * 16 + x) * 3);
        return (b[i] | b[i + 1] | b[i + 2]) != 0;
    };

    // Head: a block in the top-right of the sprite box, well above the ground.
    uint8_t headPixels = 0;
    for (uint8_t x = 4; x <= 6; x++)
        for (uint8_t y = TOP; y <= TOP + 1; y++)
            if (lit(x, y)) headPixels++;
    ASSERT(headPixels >= 5, "no head block");

    // Tail: the far-left column is lit only on the upper body, clear of the legs.
    ASSERT(lit(0, TOP + 2), "no raised tail tip");
    ASSERT(!lit(0, GROUND_Y), "the tail reaches the floor");

    // Legs: two lit pixels on the feet row with a gap or a pair, never a solid bar.
    uint8_t feet = 0;
    for (uint8_t x = 0; x < 16; x++) if (lit(x, GROUND_Y)) feet++;
    ASSERT_EQ(feet, 2u, "the runner does not stand on exactly two feet");
    PASS();
}

// ── Pong ──────────────────────────────────────────────────────────────────────

void test_pong_rallies_and_scores() {
    TEST("pong rallies, scores, and reaches the multi-ball state");
    // A rally that never breaks and a rally that never happens are both dead
    // screens. The paddle aim error is in /16 px for exactly this reason: with a
    // whole-pixel error the paddles missed a third of returns, rallies averaged
    // under three and the extra balls were never earned.
    int sawMultiBall = 0, sawScore = 0;
    for (uint32_t seed = 1; seed <= 12; seed++) {
        ge.begin(Game::PONG, 0, seed * 2654435761u | 1u);
        uint32_t t = 0;
        uint8_t peak = 0, prev = 0;
        for (int i = 0; i < 8000; i++) {
            t += GameEngine::PONG_STEP_MS;
            ge.update(t);
            const uint8_t r = ge.pongRallies();
            if (r > peak) peak = r;
            if (r == 0 && prev > 0) sawScore++;
            prev = r;
        }
        if (peak >= 9) sawMultiBall++;
    }
    ASSERT(sawMultiBall >= 8, "rallies rarely got long enough to earn a third ball");
    ASSERT(sawScore > 50, "points are never conceded — the rally never breaks");
    PASS();
}

void test_pong_ball_stays_in_the_field() {
    TEST("the pong ball never leaves the panel");
    for (uint32_t seed = 1; seed <= 10; seed++) {
        ge.begin(Game::PONG, 0, seed * 40503u + 7);
        uint32_t t = 0;
        for (int i = 0; i < 6000; i++) {
            t += GameEngine::PONG_STEP_MS;
            ge.update(t);
            // Draw::px clips silently, so an escaped ball shows up as a frame
            // with nothing but paddles on it.
            ASSERT(litPixels(ge.buffer()) >= 8, "frame lost its ball");
        }
    }
    PASS();
}

// ── Breakout ──────────────────────────────────────────────────────────────────

void test_breakout_clears_levels() {
    TEST("brick breaker clears fields and moves on");
    ge.begin(Game::BREAKOUT, 0, 0xB4EA);
    uint32_t t = 0;
    for (int i = 0; i < 30000; i++) { t += GameEngine::BREAKOUT_STEP_MS; ge.update(t); }
    ASSERT(ge.breakoutLevel() > 3, "the paddle never cleared a field");
    PASS();
}

void test_breakout_never_stalls() {
    TEST("brick breaker always has bricks and a ball on screen");
    // The failure mode to catch is a ball trapped bouncing between two walls
    // with the field already empty, which renders as an almost-black panel.
    for (uint32_t seed = 1; seed <= 10; seed++) {
        ge.begin(Game::BREAKOUT, 0, seed * 2654435761u | 1u);
        uint32_t t = 0;
        for (int i = 0; i < 12000; i++) {
            t += GameEngine::BREAKOUT_STEP_MS;
            ge.update(t);
            ASSERT(ge.breakoutBricksLeft() > 0, "field emptied without reloading");
            ASSERT(litPixels(ge.buffer()) >= 6, "panel went nearly dark");
        }
    }
    PASS();
}

// ── Shared rendering rules ────────────────────────────────────────────────────

void test_no_game_has_a_bright_background() {
    TEST("no game fills the panel with a background bright enough to compete");
    // On a real WS2812B panel two LIT colours of similar lightness are far
    // harder to tell apart than a lit colour against unlit pixels — black is
    // switched-off pixels, and it is the strongest contrast the panel has.
    //
    // Regression: Dino filled the screen with a pale "day" sky (20,24,30) and
    // drew a dark grey dino (60,70,80) on it. Only about 3x apart in luma, and
    // on the hardware the sprite was genuinely hard to pick out. Invaders'
    // (0,0,6) deep-space tint is fine by the same measure — it is effectively
    // off, and survives this check on purpose.
    //
    // The rule: whatever colour dominates the panel is the background, and a
    // background must be nearly black.
    static const uint8_t BG_MAX_CHANNEL = 12;

    for (uint8_t g = 0; g < (uint8_t)Game::COUNT; g++) {
        const uint16_t iv = stepMs((Game)g);
        ge.begin((Game)g, 0, 0xBACC0 + g);
        uint32_t t = 0;

        for (int i = 0; i < 600; i++) {
            t += iv;
            ge.update(t);
            const uint8_t* b = ge.buffer();

            // Find the most common colour and how much of the panel it covers.
            uint16_t bestCount = 0;
            uint8_t  bestR = 0, bestG = 0, bestB = 0;
            for (uint16_t p = 0; p < 256; p++) {
                const uint8_t r = b[p * 3], gg = b[p * 3 + 1], bb = b[p * 3 + 2];
                uint16_t count = 0;
                for (uint16_t q = 0; q < 256; q++)
                    if (b[q * 3] == r && b[q * 3 + 1] == gg && b[q * 3 + 2] == bb) count++;
                if (count > bestCount) { bestCount = count; bestR = r; bestG = gg; bestB = bb; }
            }

            // Under half the panel means there is no background fill to judge.
            if (bestCount < 128) continue;

            uint8_t peak = bestR;
            if (bestG > peak) peak = bestG;
            if (bestB > peak) peak = bestB;
            if (peak > BG_MAX_CHANNEL) {
                ASSERT(false, "a game filled the panel with a bright background");
                return;
            }
        }
    }
    PASS();
}

int main() {
    printf("=== GameEngine Unit Tests ===\n");
    test_determinism();
    test_no_blank_frames();
    test_update_respects_step_interval();
    test_every_game_index_is_reachable();
    test_snake_never_self_overlaps();
    test_snake_grows_and_survives();
    test_invaders_campaign_advances();
    test_invaders_formation_grows_with_level();
    test_invaders_has_bosses();
    test_invaders_win_rate_matches_the_dial();
    test_invaders_win_chance_is_tunable();
    test_dino_never_hits_an_obstacle();
    test_dino_jumps_and_lands();
    test_dino_stays_on_screen();
    test_dino_sprite_reads_as_a_dinosaur();
    test_pong_rallies_and_scores();
    test_pong_ball_stays_in_the_field();
    test_breakout_clears_levels();
    test_breakout_never_stalls();
    test_no_game_has_a_bright_background();
    SUMMARY("GameEngine");
}
