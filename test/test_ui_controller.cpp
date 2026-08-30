#include "UiController.h"
#include "Draw.h"
#include "test_harness.h"
#include <string.h>

// UiController's navigation was previously covered only through the WASM
// harness, which needs a build of the artifact and cannot run in `make test`.
// The OTA flow is the reason that gap now matters: it is the one screen where a
// wrong transition reflashes the device, so it gets asserted natively too.

static UiController ui;

using Screen   = UiController::Screen;
using OtaPhase = UiController::OtaPhase;

static uint8_t buf[Draw::SIZE];

static uint16_t litPixels() {
    uint16_t n = 0;
    for (uint16_t i = 0; i < 256; i++)
        if (buf[i * 3] | buf[i * 3 + 1] | buf[i * 3 + 2]) n++;
    return n;
}

// Walks the settings list to the UPDATE row and opens it.
static void openUpdate() {
    ui.enterSettings(0);
    while (ui.screen() == Screen::SETTINGS_MENU) {
        ui.press(0);
        if (ui.screen() == Screen::UPDATE) return;
        // Not the update row — back out and try the next one.
        ui.longPress(0);
        ui.turn(1, 0);
    }
}

void test_settings_reaches_update() {
    TEST("the settings list has a reachable UPDATE entry");
    openUpdate();
    ASSERT_EQ((int)ui.screen(), (int)Screen::UPDATE, "never landed on the update screen");
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::CONFIRM, "update did not open on the confirmation");
    PASS();
}

void test_update_defaults_to_no() {
    TEST("the confirmation opens on NO and backs out without asking for a flash");
    openUpdate();
    // Confirming immediately must be the harmless choice: this reflashes the
    // device, so the destructive option is never one click from the menu.
    ui.press(0);
    ASSERT(!ui.otaRequested(), "an update was requested from the default choice");
    ASSERT_EQ((int)ui.screen(), (int)Screen::SETTINGS_MENU, "did not return to settings");
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::IDLE, "phase left dangling");
    PASS();
}

void test_update_requests_only_after_yes() {
    TEST("turning to YES and confirming raises the request exactly once");
    openUpdate();
    ui.turn(1, 0);                       // NO -> YES
    ui.press(0);
    ASSERT(ui.otaRequested(), "confirming YES did not request an update");
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::RUNNING, "did not enter the progress screen");

    ui.clearOtaRequest();
    ASSERT(!ui.otaRequested(), "the request did not clear");
    PASS();
}

void test_update_is_not_cancellable_mid_write() {
    TEST("neither gesture escapes a flash write in progress");
    openUpdate();
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::RUNNING, "not running");

    // Backing out here would leave a half-written slot with the UI claiming
    // otherwise. Both inputs must be inert until the firmware reports a result.
    ui.press(0);
    ASSERT_EQ((int)ui.screen(), (int)Screen::UPDATE, "press escaped a running update");
    const bool toMainMenu = ui.longPress(0);
    ASSERT(!toMainMenu, "long-press tried to exit to the main menu mid-write");
    ASSERT_EQ((int)ui.screen(), (int)Screen::UPDATE, "long-press escaped a running update");
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::RUNNING, "phase changed mid-write");
    PASS();
}

void test_update_progress_and_results() {
    TEST("progress clamps, and both verdicts are exits");
    openUpdate();
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();

    ui.setOtaProgress(0);
    ASSERT_EQ(ui.otaProgress(), 0u, "0% not accepted");
    ui.setOtaProgress(57);
    ASSERT_EQ(ui.otaProgress(), 57u, "57% not accepted");
    ui.setOtaProgress(200);
    ASSERT_EQ(ui.otaProgress(), 100u, "progress above 100 was not clamped");

    ui.setOtaResult(false);
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::FAILED, "failure not recorded");
    ui.press(0);
    ASSERT_EQ((int)ui.screen(), (int)Screen::SETTINGS_MENU, "failure screen did not exit");

    openUpdate();
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();
    ui.setOtaResult(true);
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::SUCCEEDED, "success not recorded");
    ASSERT_EQ(ui.otaProgress(), 100u, "success did not show a full bar");
    PASS();
}

void test_update_screens_all_render() {
    TEST("every OTA phase draws something legible");
    // A blank panel during a reflash is indistinguishable from a hung device.
    openUpdate();
    ui.render(buf, 0);
    ASSERT(litPixels() > 6, "the confirmation rendered nearly blank");

    ui.turn(1, 0);
    ui.render(buf, 0);
    const uint16_t yesLit = litPixels();
    ASSERT(yesLit > 6, "the YES state rendered nearly blank");

    ui.press(0);
    ui.clearOtaRequest();
    for (uint8_t pct = 0; pct <= 100; pct = (uint8_t)(pct + 5)) {
        ui.setOtaProgress(pct);
        ui.render(buf, 0);
        ASSERT(litPixels() > 3, "a progress frame rendered nearly blank");
    }

    ui.setOtaResult(true);
    ui.render(buf, 0);
    ASSERT(litPixels() > 6, "the OK screen rendered nearly blank");

    ui.setOtaResult(false);
    ui.render(buf, 0);
    ASSERT(litPixels() > 6, "the ERR screen rendered nearly blank");
    PASS();
}

void test_ota_result_screens_are_distinguishable() {
    TEST("OK and ERR do not render the same pixels");
    // The one thing a result screen has to do is tell you which result it is.
    openUpdate();
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();

    uint8_t okFrame[Draw::SIZE];
    ui.setOtaResult(true);
    ui.render(okFrame, 0);

    ui.setOtaResult(false);
    ui.render(buf, 0);

    ASSERT(memcmp(okFrame, buf, Draw::SIZE) != 0, "success and failure look identical");
    PASS();
}

void test_ip_screen_shows_every_octet() {
    TEST("the IP screen scrolls the whole address in the 5x7 font");
    // You cannot OTA a device whose address you do not know, and it is otherwise
    // only ever printed to a serial console nobody has attached.
    ui.enterSettings(0);
    ui.setIpAddress(192, 168, 0, 113);

    uint8_t guard = 0;
    while (ui.screen() == Screen::SETTINGS_MENU && guard++ < 10) {
        ui.press(0);
        if (ui.screen() == Screen::IP_VIEW) break;
        ui.longPress(0);
        ui.turn(1, 0);
    }
    ASSERT_EQ((int)ui.screen(), (int)Screen::IP_VIEW, "never reached the IP screen");

    ui.render(buf, 0);
    ASSERT(litPixels() > 8, "the IP screen rendered nearly blank");

    // 5x7 glyphs live in rows 4..10; nothing may bleed outside that band.
    for (uint8_t y = 0; y < 16; y++) {
        if (y >= 4 && y <= 10) continue;
        for (uint8_t x = 0; x < 16; x++) {
            const uint16_t i = (uint16_t)((y * 16 + x) * 3);
            ASSERT((buf[i] | buf[i + 1] | buf[i + 2]) == 0, "ink outside the text band");
        }
    }

    // It must dwell first, then move, then keep moving — a screen that never
    // scrolls only ever shows the first two characters of the address.
    uint8_t atDwell[Draw::SIZE];
    ui.render(atDwell, 400);                 // inside IP_DWELL_MS
    ui.render(buf, 0);
    ASSERT(memcmp(atDwell, buf, Draw::SIZE) == 0, "scrolled during the dwell");

    ui.render(buf, 3000);
    ASSERT(memcmp(atDwell, buf, Draw::SIZE) != 0, "never scrolled after the dwell");

    uint8_t later[Draw::SIZE];
    ui.render(later, 6000);
    ASSERT(memcmp(later, buf, Draw::SIZE) != 0, "scroll stalled");

    // Every octet has to actually appear over a full pass, or the address is
    // unreadable no matter how good the font is. Sample a whole cycle and count
    // the distinct frames — a stuck marquee collapses this to a handful.
    uint8_t distinct = 0;
    uint8_t prev[Draw::SIZE];
    ui.render(prev, 1000);
    for (uint32_t t = 1000; t <= 9000; t += 250) {
        ui.render(buf, t);
        if (memcmp(prev, buf, Draw::SIZE) != 0) { distinct++; memcpy(prev, buf, Draw::SIZE); }
    }
    ASSERT(distinct > 12, "the address never travelled across the panel");

    // A different address must look different, or the screen is decorative.
    uint8_t first[Draw::SIZE];
    ui.render(first, 0);
    ui.setIpAddress(10, 0, 42, 7);
    ui.render(buf, 0);
    ASSERT(memcmp(first, buf, Draw::SIZE) != 0, "the screen ignored the address");
    PASS();
}

void test_ip_screen_says_so_when_offline() {
    TEST("no WiFi renders a distinct state, not a stale address");
    ui.enterSettings(0);
    ui.setIpAddress(192, 168, 0, 113);
    uint8_t guard = 0;
    while (ui.screen() == Screen::SETTINGS_MENU && guard++ < 8) {
        ui.press(0);
        if (ui.screen() == Screen::IP_VIEW) break;
        ui.longPress(0);
        ui.turn(1, 0);
    }
    ui.render(buf, 0);
    uint8_t online[Draw::SIZE];
    memcpy(online, buf, Draw::SIZE);

    // Showing the last known address while offline would be actively misleading:
    // it is exactly the address someone would then try to OTA to.
    ui.setNetworkDown();
    ASSERT(!ui.hasIpAddress(), "still claims to have an address");
    ui.render(buf, 0);
    ASSERT(memcmp(online, buf, Draw::SIZE) != 0, "offline looks identical to online");
    ASSERT(litPixels() > 6, "the offline state rendered nearly blank");
    PASS();
}

void test_games_menu_matches_the_game_enum() {
    TEST("every games-menu row starts the game at its index");
    // GAME_ITEMS is indexed straight into the Game enum, so a row inserted in
    // one without the other silently starts the wrong game.
    for (uint8_t g = 0; g < (uint8_t)Game::COUNT; g++) {
        ui.enterGames(0);
        for (uint8_t i = 0; i < g; i++) ui.turn(1, 0);
        ui.press(0);
        ASSERT_EQ((int)ui.screen(), (int)Screen::GAME_PLAYING, "row did not start a game");
        ASSERT_EQ((int)ui.currentGame(), (int)g, "row started the wrong game");
        ui.longPress(0);
    }
    // One past the last game is CYCLE ALL.
    ui.enterGames(0);
    for (uint8_t i = 0; i < (uint8_t)Game::COUNT; i++) ui.turn(1, 0);
    ui.press(0);
    ASSERT(ui.cycling(), "the row after the last game is not CYCLE ALL");
    PASS();
}

void test_root_screens_exit_to_the_main_menu() {
    TEST("long-press escapes to the main menu only from a root screen");
    ui.enterGames(0);
    ASSERT(ui.longPress(0), "the game list should hand back to the main menu");

    ui.enterGames(0);
    ui.press(0);                                  // into a game
    ASSERT(!ui.longPress(0), "a running game should drop to the game list");
    ASSERT_EQ((int)ui.screen(), (int)Screen::GAMES_MENU, "did not land on the game list");

    ui.enterSettings(0);
    ASSERT(ui.longPress(0), "the settings list should hand back to the main menu");
    PASS();
}

int main() {
    printf("=== UiController Unit Tests ===\n");
    test_settings_reaches_update();
    test_update_defaults_to_no();
    test_update_requests_only_after_yes();
    test_update_is_not_cancellable_mid_write();
    test_update_progress_and_results();
    test_update_screens_all_render();
    test_ip_screen_shows_every_octet();
    test_ip_screen_says_so_when_offline();
    test_ota_result_screens_are_distinguishable();
    test_games_menu_matches_the_game_enum();
    test_root_screens_exit_to_the_main_menu();
    SUMMARY("UiController");
}
