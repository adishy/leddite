#include "UiController.h"
#include "Draw.h"
#include "BrightnessModel.h"
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
    // The device always knows its address by the time this screen matters, and
    // the window is refused without one — see test_update_refuses_without_an_ip.
    ui.setIpAddress(192, 168, 0, 113);
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
    TEST("turning to YES and confirming opens the upload window exactly once");
    openUpdate();
    ui.turn(1, 0);                       // NO -> YES
    ui.press(0);
    ASSERT(ui.otaRequested(), "confirming YES did not ask the firmware for a window");
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::WAITING,
              "did not enter the waiting screen");

    ui.clearOtaRequest();
    ASSERT(!ui.otaRequested(), "the request did not clear");
    PASS();
}

void test_update_refuses_without_an_ip() {
    TEST("with no address there is nothing to browse to, so YES fails outright");
    ui.setNetworkDown();
    ui.enterSettings(0);
    while (ui.screen() == Screen::SETTINGS_MENU) {
        ui.press(0);
        if (ui.screen() == Screen::UPDATE) break;
        ui.longPress(0);
        ui.turn(1, 0);
    }
    ui.turn(1, 0);                       // NO -> YES
    ui.press(0);
    // Opening a server nobody can reach, and showing a URL that is not the
    // device's, would be worse than saying no.
    ASSERT(!ui.otaRequested(), "asked the firmware to open a window with no address");
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::FAILED, "did not fail outright");
    PASS();
}

void test_waiting_window_is_cancellable_and_expires() {
    TEST("the upload window closes on a press, on a long-press, and on its own");
    openUpdate();
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::WAITING, "not waiting");

    // Nothing has been written yet, so unlike a running write this is safe to
    // abandon — and it must be, or a mistaken YES leaves a server up.
    ui.press(0);
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::IDLE, "press did not close the window");
    ASSERT_EQ((int)ui.screen(), (int)Screen::SETTINGS_MENU, "press did not return to settings");

    openUpdate(); ui.turn(1, 0); ui.press(0); ui.clearOtaRequest();
    ui.longPress(0);
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::IDLE, "long-press did not close the window");

    // And it must not stay open because somebody walked away.
    openUpdate(); ui.turn(1, 0); ui.press(0); ui.clearOtaRequest();
    ui.update(UiController::OTA_WINDOW_MS - 1);
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::WAITING, "closed early");
    ui.update(UiController::OTA_WINDOW_MS);
    ASSERT_EQ((int)ui.otaPhase(), (int)OtaPhase::IDLE, "the window never timed out");
    ASSERT_EQ((int)ui.screen(), (int)Screen::SETTINGS_MENU, "timeout left the screen up");
    PASS();
}

void test_waiting_screen_shows_the_url() {
    TEST("the waiting screen draws the address you must type, and it scrolls");
    openUpdate();
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();

    uint8_t a[Draw::SIZE], b[Draw::SIZE];
    ui.render(a, 0);
    // Past the dwell and far enough for the marquee to have moved on.
    ui.render(b, 4000);

    int litA = 0;
    for (int i = 0; i < (int)Draw::SIZE; i += 3)
        if (a[i] || a[i + 1] || a[i + 2]) litA++;
    ASSERT(litA > 12, "the waiting screen is essentially blank");
    ASSERT(memcmp(a, b, Draw::SIZE) != 0,
           "the URL never moves — a 20-character string cannot fit 16px unscrolled");
    PASS();
}

void test_update_is_not_cancellable_mid_write() {
    TEST("neither gesture escapes a flash write in progress");
    openUpdate();
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();
    // The upload has started: the firmware reports bytes, which is what moves
    // the screen from waiting to writing.
    ui.setOtaProgress(1);
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

void test_screens_survive_the_lowest_brightness() {
    TEST("every screen still lights pixels at brightness level 1");
    // FastLED's global brightness multiplies each channel by level/255, and
    // level 1 is 6 — so any channel below ~43 floors to zero and is simply not
    // on the panel. A contact sheet renders the RAW buffer and cannot show this,
    // which is how the OTA progress track shipped at (30,30,38): correct in
    // review, invisible on a device set to level 1, leaving the screen a single
    // lone digit exactly when you are watching for progress.
    //
    // The lesson is that at low global brightness you can only modulate by
    // COVERAGE, not by value — docs/adr/0012 one step further on.
    const uint8_t scale = BrightnessModel::levelToFastLED(BrightnessModel::MIN_LEVEL);

    // Counts pixels that are still lit after the global scale, within a row
    // band. Banding matters: a whole-screen count is not discriminating here,
    // because the percentage digit alone clears any sensible threshold while the
    // bar beside it is completely dark — which is exactly the bug.
    auto litRowsAfterScaling = [&](uint8_t y0, uint8_t y1) {
        uint16_t n = 0;
        for (uint8_t y = y0; y <= y1; y++)
            for (uint8_t x = 0; x < 16; x++) {
                const uint16_t i = (uint16_t)((y * 16 + x) * 3);
                const uint16_t r = ((uint16_t)buf[i]     * scale) >> 8;
                const uint16_t g = ((uint16_t)buf[i + 1] * scale) >> 8;
                const uint16_t b = ((uint16_t)buf[i + 2] * scale) >> 8;
                if (r | g | b) n++;
            }
        return n;
    };
    auto litAfterScaling = [&]() { return litRowsAfterScaling(0, 15); };

    // The OTA progress screen at 0% is the specific regression: the percentage
    // digit alone is indistinguishable from a hung device, so the bar must
    // survive too.
    ui.enterSettings(0);
    uint8_t guard = 0;
    while (ui.screen() == Screen::SETTINGS_MENU && guard++ < 10) {
        ui.press(0);
        if (ui.screen() == Screen::UPDATE) break;
        ui.longPress(0);
        ui.turn(1, 0);
    }
    ASSERT_EQ((int)ui.screen(), (int)Screen::UPDATE, "never reached the update screen");
    ui.turn(1, 0);
    ui.press(0);
    ui.clearOtaRequest();

    // The bar occupies rows 10-12; the percentage digit sits well above it.
    static const uint8_t BAR_Y0 = 10, BAR_Y1 = 12;

    ui.setOtaProgress(0);
    ui.render(buf, 0);
    const uint16_t bar0 = litRowsAfterScaling(BAR_Y0, BAR_Y1);
    // A 1px fill floor alone is 3 pixels. The empty track has to contribute
    // more than that, or an empty bar is indistinguishable from no bar.
    ASSERT(bar0 >= 6, "the empty progress track vanishes at the lowest brightness");

    // And it must still grow, or the bar conveys nothing at level 1.
    ui.setOtaProgress(70);
    ui.render(buf, 0);
    ASSERT(litRowsAfterScaling(BAR_Y0, BAR_Y1) > bar0,
           "the bar does not fill at the lowest brightness");

    // Every other screen must keep something on the panel too.
    struct { const char* name; Screen want; } SCREENS[] = {
        { "brightness", Screen::BRIGHTNESS_EDIT },
        { "units",      Screen::UNITS_EDIT },
        { "IP",         Screen::IP_VIEW },
    };
    for (const auto& sc : SCREENS) {
        ui.enterSettings(0);
        ui.setIpAddress(192, 168, 0, 113);
        guard = 0;
        while (ui.screen() == Screen::SETTINGS_MENU && guard++ < 10) {
            ui.press(0);
            if (ui.screen() == sc.want) break;
            ui.longPress(0);
            ui.turn(1, 0);
        }
        ASSERT_EQ((int)ui.screen(), (int)sc.want, "never reached a settings screen");
        ui.render(buf, 0);
        ASSERT(litAfterScaling() >= 4, "a settings screen goes dark at level 1");
    }
    PASS();
}

void test_encoder_is_inverted_and_halved_on_lists() {
    TEST("the knob takes two clicks per row and moves the list, not the cursor");
    ui.enterSettings(0);

    // One detent is half a row: the menu must not move at all yet. This is the
    // whole point — one click per row overshoots on a five-item list.
    ui.encoderTurn(1, 0);
    ui.press(0);
    ASSERT_EQ((int)ui.screen(), (int)Screen::BRIGHTNESS_EDIT,
              "a single detent already moved the selection off row 0");
    ui.longPress(0);

    // Two detents is one row, and the list moves the opposite way to the knob:
    // +2 must land on the row ABOVE row 0, which on a wrapping list is the last.
    ui.enterSettings(0);
    ui.encoderTurn(2, 0);
    ui.press(0);
    ASSERT_EQ((int)ui.screen(), (int)Screen::UPDATE,
              "two detents clockwise did not move up one row to UPDATE");
    ui.longPress(0);

    // And the other way round: -2 goes down one row, to PLACE.
    ui.enterSettings(0);
    ui.encoderTurn(-2, 0);
    ui.press(0);
    ASSERT_EQ((int)ui.screen(), (int)Screen::PLACES_MENU,
              "two detents anticlockwise did not move down one row to PLACE");
    ui.longPress(0);

    // Half-turns accumulate rather than being discarded...
    ui.enterSettings(0);
    ui.encoderTurn(-1, 0);
    ui.encoderTurn(-1, 0);
    ui.press(0);
    ASSERT_EQ((int)ui.screen(), (int)Screen::PLACES_MENU,
              "two separate detents did not add up to one row");
    // ...but never across a screen change, or a stale half-turn steers the next
    // menu on its first click.
    ui.longPress(0);
    ui.enterSettings(0);
    ui.encoderTurn(-1, 0);
    ui.longPress(0);
    ui.enterSettings(0);
    ui.encoderTurn(-1, 0);
    ui.press(0);
    ASSERT_EQ((int)ui.screen(), (int)Screen::BRIGHTNESS_EDIT,
              "a half-turn leaked across a screen change");
    ui.longPress(0);
    PASS();
}

void test_encoder_is_raw_on_value_editors() {
    TEST("value editors keep one click per unit, in the direction turned");
    ui.enterSettings(0);
    ui.press(0);                                    // BRIGHTNESS_EDIT
    ASSERT_EQ((int)ui.screen(), (int)Screen::BRIGHTNESS_EDIT, "not on brightness");

    ui.setBrightnessLevel(5);
    ui.encoderTurn(1, 0);
    ASSERT_EQ((int)ui.brightnessLevel(), 6,
              "one detent did not raise the level by one — the editor is not a list");
    ui.encoderTurn(-1, 0);
    ASSERT_EQ((int)ui.brightnessLevel(), 5, "one detent back did not lower it by one");
    ui.longPress(0);
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
    test_update_refuses_without_an_ip();
    test_waiting_window_is_cancellable_and_expires();
    test_waiting_screen_shows_the_url();
    test_update_is_not_cancellable_mid_write();
    test_update_progress_and_results();
    test_update_screens_all_render();
    test_ip_screen_shows_every_octet();
    test_ip_screen_says_so_when_offline();
    test_screens_survive_the_lowest_brightness();
    test_ota_result_screens_are_distinguishable();
    test_encoder_is_inverted_and_halved_on_lists();
    test_encoder_is_raw_on_value_editors();
    test_games_menu_matches_the_game_enum();
    test_root_screens_exit_to_the_main_menu();
    SUMMARY("UiController");
}
