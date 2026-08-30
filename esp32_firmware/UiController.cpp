#include "UiController.h"
#include "BrightnessModel.h"
#include "Draw.h"
#include "SmallTextRenderer.h"
#include "TextRenderer.h"
#include <string.h>

// ── Menu contents ─────────────────────────────────────────────────────────────
//
// Labels of 4 characters or fewer render without scrolling (SmallTextRenderer
// advances 4px per character into a 16px row). Longer ones scroll, which is
// handled by ListMenu — but keeping the common cases short keeps the list calm.

// Order must match the Game enum: startGame() indexes into it directly.
static const MenuItem GAME_ITEMS[] = {
    { "SNAKE",      60, 220,  90 },
    { "INVADERS",  255,  90,  90 },
    { "DINO",      215, 195, 110 },   // desert sand — SNAKE already owns mint
    { "PONG",      120, 200, 255 },
    { "BRICKS",    255, 170,  60 },
    { "CYCLE ALL", 200, 120, 255 },
};
static const uint8_t GAME_COUNT  = 6;
static const uint8_t CYCLE_INDEX = 5;   // "CYCLE ALL" is the last entry

static const MenuItem SETTINGS_ITEMS[] = {
    { "BRIGHTNESS", 255, 200,  80 },
    { "PLACE",      120, 220, 255 },
    { "UNITS",      200, 255, 140 },
    { "IP",         120, 255, 220 },
    { "UPDATE",     255, 120, 160 },
};
static const uint8_t SETTINGS_COUNT = 5;

enum : uint8_t {
    SET_BRIGHTNESS = 0, SET_PLACE = 1, SET_UNITS = 2, SET_IP = 3, SET_UPDATE = 4
};

// ── Construction ──────────────────────────────────────────────────────────────

UiController::UiController() {
    buildPlacesMenu();
    gamesMenu.begin(GAME_ITEMS, GAME_COUNT, 0, 0);
    settingsMenu.begin(SETTINGS_ITEMS, SETTINGS_COUNT, 0, 0);
    placesMenu.begin(placeItems, Places::COUNT, place, 0);
}

void UiController::buildPlacesMenu() {
    // Derived from the Places table rather than duplicated, so adding a location
    // in one place is enough.
    for (uint8_t i = 0; i < Places::COUNT; i++) {
        placeItems[i].label = Places::get(i).code;
        placeItems[i].r = 120;
        placeItems[i].g = 200;
        placeItems[i].b = 255;
    }
}

// ── Entry points ──────────────────────────────────────────────────────────────

void UiController::enterGames(uint32_t nowMs) {
    cur      = Screen::GAMES_MENU;
    cycleAll = false;
    gamesMenu.begin(GAME_ITEMS, GAME_COUNT, 0, nowMs);
}

void UiController::enterSettings(uint32_t nowMs) {
    cur = Screen::SETTINGS_MENU;
    settingsMenu.begin(SETTINGS_ITEMS, SETTINGS_COUNT, 0, nowMs);
}

void UiController::enterWeather(uint32_t nowMs) {
    cur              = Screen::WEATHER;
    weatherEnteredMs = nowMs;
}

// ── Setting mutators ──────────────────────────────────────────────────────────

void UiController::setBrightnessLevel(uint8_t level) {
    const uint8_t v = BrightnessModel::clampLevel((int)level);
    if (v != brightness) { brightness = v; dirty = true; }
}

void UiController::setPlaceIndex(uint8_t index) {
    const uint8_t v = (index < Places::COUNT) ? index : Places::DEFAULT_INDEX;
    if (v != place) { place = v; dirty = true; }
}

void UiController::setUnit(TempUnit u) {
    if (u != tempUnit) { tempUnit = u; dirty = true; }
}

// ── Games ─────────────────────────────────────────────────────────────────────

void UiController::startGame(uint8_t gameIndex, uint32_t nowMs) {
    // Seed from the clock so successive plays differ, but never zero —
    // xorshift32 locks up on a zero state.
    const uint32_t seed = (nowMs * 2654435761u) | 1u;
    engine.begin((Game)(gameIndex % (uint8_t)Game::COUNT), nowMs, seed);
    cur = Screen::GAME_PLAYING;
}

// ── Input ─────────────────────────────────────────────────────────────────────

void UiController::turn(int delta, uint32_t nowMs) {
    if (delta == 0) return;

    switch (cur) {
        case Screen::GAMES_MENU:
            gamesMenu.turn(delta, nowMs);
            break;

        case Screen::GAME_PLAYING:
            // Turning skips to the next game. Under CYCLE ALL this also resets
            // the dwell so the new game gets its full slot.
            if (cycleAll) {
                cycleIndex   = (uint8_t)(((int)cycleIndex + (int)Game::COUNT + delta) % (int)Game::COUNT);
                cycleStartMs = nowMs;
                startGame(cycleIndex, nowMs);
            } else {
                gamesMenu.turn(delta, nowMs);
                if (gamesMenu.selected() != CYCLE_INDEX) startGame(gamesMenu.selected(), nowMs);
            }
            break;

        case Screen::SETTINGS_MENU:
            settingsMenu.turn(delta, nowMs);
            break;

        case Screen::BRIGHTNESS_EDIT:
            setBrightnessLevel((uint8_t)BrightnessModel::clampLevel((int)brightness + delta));
            break;

        case Screen::PLACES_MENU:
            placesMenu.turn(delta, nowMs);
            break;

        case Screen::UNITS_EDIT:
            // Two options, so any turn toggles.
            setUnit(tempUnit == TempUnit::CELSIUS ? TempUnit::FAHRENHEIT : TempUnit::CELSIUS);
            break;

        case Screen::UPDATE:
            // Only the confirmation is steerable. There is no cancelling a flash
            // write partway through, and the result screens have one exit.
            if (otaSt == OtaPhase::CONFIRM) otaYes = !otaYes;
            break;

        case Screen::WEATHER:
        default:
            break;
    }
}

void UiController::press(uint32_t nowMs) {
    switch (cur) {
        case Screen::GAMES_MENU: {
            const uint8_t sel = gamesMenu.selected();
            if (sel == CYCLE_INDEX) {
                cycleAll     = true;
                cycleIndex   = 0;
                cycleStartMs = nowMs;
                startGame(cycleIndex, nowMs);
            } else {
                cycleAll = false;
                startGame(sel, nowMs);
            }
            break;
        }

        case Screen::GAME_PLAYING:
            // Restart the current round — a deliberate "shuffle" gesture.
            startGame(cycleAll ? cycleIndex : gamesMenu.selected(), nowMs);
            if (cycleAll) cycleStartMs = nowMs;
            break;

        case Screen::SETTINGS_MENU:
            switch (settingsMenu.selected()) {
                case SET_BRIGHTNESS: cur = Screen::BRIGHTNESS_EDIT; break;
                case SET_PLACE:
                    placesMenu.begin(placeItems, Places::COUNT, place, nowMs);
                    cur = Screen::PLACES_MENU;
                    break;
                case SET_UNITS:      cur = Screen::UNITS_EDIT; break;
                case SET_IP:
                    cur         = Screen::IP_VIEW;
                    ipEnteredMs = nowMs;         // anchors the scroll dwell
                    break;
                case SET_UPDATE:
                    // Always opens on NO: this reflashes the device, so the
                    // destructive choice must never be one click away.
                    otaSt  = OtaPhase::CONFIRM;
                    otaYes = false;
                    otaPct = 0;
                    cur    = Screen::UPDATE;
                    break;
                default: break;
            }
            break;

        case Screen::PLACES_MENU:
            setPlaceIndex(placesMenu.selected());
            cur = Screen::SETTINGS_MENU;
            break;

        case Screen::BRIGHTNESS_EDIT:
        case Screen::UNITS_EDIT:
        case Screen::IP_VIEW:
            cur = Screen::SETTINGS_MENU;
            break;

        case Screen::UPDATE:
            switch (otaSt) {
                case OtaPhase::CONFIRM:
                    if (otaYes) {
                        otaReq = true;                 // the firmware picks this up
                        otaSt  = OtaPhase::RUNNING;
                        otaPct = 0;
                    } else {
                        otaSt = OtaPhase::IDLE;
                        cur   = Screen::SETTINGS_MENU;
                    }
                    break;
                case OtaPhase::SUCCEEDED:
                case OtaPhase::FAILED:
                    otaSt = OtaPhase::IDLE;
                    cur   = Screen::SETTINGS_MENU;
                    break;
                case OtaPhase::RUNNING:
                default:
                    break;                             // a flash write is not cancellable
            }
            break;

        case Screen::WEATHER:
        default:
            break;
    }
}

bool UiController::longPress(uint32_t nowMs) {
    switch (cur) {
        // Root screens: nowhere further up, so the caller returns to the main menu.
        case Screen::GAMES_MENU:
        case Screen::SETTINGS_MENU:
        case Screen::WEATHER:
            return true;

        // A running game drops back to the game list, not all the way out.
        case Screen::GAME_PLAYING:
            cycleAll = false;
            gamesMenu.begin(GAME_ITEMS, GAME_COUNT, gamesMenu.selected(), nowMs);
            cur = Screen::GAMES_MENU;
            return false;

        // Editors drop back to the settings list, discarding nothing —
        // values are applied live, so backing out keeps the change.
        case Screen::BRIGHTNESS_EDIT:
        case Screen::PLACES_MENU:
        case Screen::UNITS_EDIT:
        case Screen::IP_VIEW:
            cur = Screen::SETTINGS_MENU;
            return false;

        // Backing out of a flash write in progress would leave a half-written
        // slot with the UI claiming otherwise, so the gesture is ignored until
        // the firmware reports a result.
        case Screen::UPDATE:
            if (otaSt == OtaPhase::RUNNING) return false;
            otaSt = OtaPhase::IDLE;
            cur   = Screen::SETTINGS_MENU;
            return false;

        default:
            return true;
    }
}

// ── Frame ─────────────────────────────────────────────────────────────────────

void UiController::update(uint32_t nowMs) {
    if (cur != Screen::GAME_PLAYING) return;

    if (cycleAll && (uint32_t)(nowMs - cycleStartMs) >= CYCLE_INTERVAL_MS) {
        cycleIndex   = (uint8_t)((cycleIndex + 1) % (uint8_t)Game::COUNT);
        cycleStartMs = nowMs;
        startGame(cycleIndex, nowMs);
    }

    engine.update(nowMs);
}

void UiController::renderUnits(uint8_t* buf) const {
    Draw::clear(buf);

    const bool    c   = (tempUnit == TempUnit::CELSIUS);
    const uint8_t col[3] = { 200, 255, 140 };

    uint8_t  txt[16 * SmallTextRenderer::CHAR_HEIGHT * 3];
    uint16_t w = 0, h = 0;
    SmallTextRenderer::renderText(c ? "C" : "F", txt, w, h, col);
    Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 4);

    // Two pips showing which of the two options is active.
    Draw::rect(buf, 4,  11, 3, 2, c ? col[0] : 30, c ? col[1] : 30, c ? col[2] : 34);
    Draw::rect(buf, 9,  11, 3, 2, c ? 30 : col[0], c ? 30 : col[1], c ? 34 : col[2]);
}

// ── Network identity ──────────────────────────────────────────────────────────

void UiController::setIpAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    ip[0] = a; ip[1] = b; ip[2] = c; ip[3] = d;
    ipValid = true;
}

void UiController::setNetworkDown() { ipValid = false; }

void UiController::drawWide(uint8_t* buf, const char* text, int16_t x, int16_t y,
                            const uint8_t* colour) {
    // One glyph at a time. Rendering the whole string first would need a
    // 15-character staging buffer — 90 x 7 x 3 = 1,890 bytes — carried purely to
    // be blitted straight out again, on a part that has 320 KB total.
    uint8_t  glyph[TextRenderer::CHAR_STRIDE * TextRenderer::CHAR_HEIGHT * 3];
    char     one[2] = { 0, 0 };
    int16_t  cx = x;

    for (const char* p = text; *p; p++) {
        one[0] = *p;
        uint16_t w = 0, h = 0;
        TextRenderer::renderText(one, glyph, w, h, colour);
        // Clipped by Draw::blit, so glyphs scrolled off either edge cost nothing.
        if (cx > -(int16_t)TextRenderer::CHAR_STRIDE && cx < (int16_t)Draw::W)
            Draw::blit(buf, glyph, w, h, cx, y);
        cx = (int16_t)(cx + TextRenderer::CHAR_STRIDE);
    }
}

int16_t UiController::ipScrollOffset(uint16_t textW, uint32_t nowMs) const {
    if (textW <= Draw::W) return 0;                    // fits: never scrolls
    const uint32_t elapsed = nowMs - ipEnteredMs;
    if (elapsed <= IP_DWELL_MS) return 0;              // dwell on the first octet
    const uint32_t travel = (uint32_t)textW + IP_GAP_PX;
    const uint32_t moved  = ((elapsed - IP_DWELL_MS) * IP_PPS) / 1000u;
    return (int16_t)-(int16_t)(moved % travel);
}

void UiController::renderIp(uint8_t* buf, uint32_t nowMs) const {
    Draw::clear(buf);

    static const uint8_t DOWN[3] = { 255, 90, 80 };
    if (!ipValid) {
        // Showing the last known address while offline would be actively
        // misleading — it is exactly the address someone would then try to OTA
        // to. The small font is right here: two short words, no scrolling.
        uint8_t  txt[16 * SmallTextRenderer::CHAR_HEIGHT * 3];
        uint16_t w = 0, h = 0;
        SmallTextRenderer::renderText("NO", txt, w, h, DOWN);
        Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 3);
        SmallTextRenderer::renderText("WIFI", txt, w, h, DOWN);
        Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 9);
        return;
    }

    // "192.168.0.113" in the 5x7 font, scrolling. The 3x4 font would fit an
    // octet per row with no movement at all, but four 4px rows stack with no
    // gutter — the digits ran together, and on the panel this is a string you
    // copy down while looking away, where glyph size beats not having to wait.
    char text[16];
    uint8_t n = 0;
    for (uint8_t i = 0; i < 4; i++) {
        const uint8_t v = ip[i];
        if (v >= 100) text[n++] = (char)('0' + v / 100);
        if (v >= 10)  text[n++] = (char)('0' + (v / 10) % 10);
        text[n++] = (char)('0' + v % 10);
        if (i < 3) text[n++] = '.';
    }
    text[n] = 0;

    static const uint8_t COL[3] = { 120, 245, 220 };
    const uint16_t w    = TextRenderer::textWidth(text);
    const int16_t  xOff = ipScrollOffset(w, nowMs);
    const int16_t  y    = (int16_t)((16 - (int16_t)TextRenderer::CHAR_HEIGHT) / 2);

    drawWide(buf, text, xOff, y, COL);
    // Second copy so the wrap has no blank stretch at the seam.
    if (w > Draw::W)
        drawWide(buf, text, (int16_t)(xOff + (int16_t)w + IP_GAP_PX), y, COL);
}

// ── OTA ───────────────────────────────────────────────────────────────────────

void UiController::setOtaProgress(uint8_t pct) {
    otaPct = pct > 100 ? 100 : pct;
    // A progress report is also the firmware saying "I am still writing", which
    // is the only thing that can move the screen back out of a stale result.
    if (otaSt != OtaPhase::RUNNING) {
        otaSt = OtaPhase::RUNNING;
        cur   = Screen::UPDATE;
    }
}

void UiController::setOtaResult(bool ok) {
    otaSt  = ok ? OtaPhase::SUCCEEDED : OtaPhase::FAILED;
    otaPct = ok ? 100 : otaPct;
    cur    = Screen::UPDATE;
}

void UiController::renderUpdate(uint8_t* buf) const {
    Draw::clear(buf);

    // Every state is a word, not a symbol: a 16x16 panel has no room for an icon
    // that reads unambiguously, and "is that arrow up or down" is exactly the
    // wrong question to be asking while a device reflashes itself (docs/adr/0011).
    uint8_t  txt[16 * SmallTextRenderer::CHAR_HEIGHT * 3];
    uint16_t w = 0, h = 0;

    static const uint8_t PINK[3]  = { 255, 120, 160 };
    static const uint8_t GREEN[3] = {  90, 235, 120 };
    static const uint8_t RED[3]   = { 255,  70,  60 };
    static const uint8_t GREY[3]  = { 120, 130, 150 };

    const uint8_t TRACK_X = 1, TRACK_W = 14, BAR_Y = 10, BAR_H = 3;

    switch (otaSt) {
        case OtaPhase::CONFIRM: {
            SmallTextRenderer::renderText("OTA", txt, w, h, PINK);
            Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 2);

            SmallTextRenderer::renderText(otaYes ? "YES" : "NO", txt, w, h,
                                          otaYes ? GREEN : GREY);
            Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 8);

            // Two pips, same idiom as the units editor, so the pair reads as a
            // two-state choice rather than as a label that happens to change.
            Draw::rect(buf, 4, 13, 3, 2, otaYes ? 30 : GREY[0],
                                          otaYes ? 30 : GREY[1],
                                          otaYes ? 34 : GREY[2]);
            Draw::rect(buf, 9, 13, 3, 2, otaYes ? GREEN[0] : 30,
                                          otaYes ? GREEN[1] : 30,
                                          otaYes ? GREEN[2] : 34);
            break;
        }

        case OtaPhase::RUNNING: {
            char pct[5];
            // No snprintf here: this file is compiled for the ESP32 too, and the
            // three cases are trivial.
            const uint8_t v = otaPct;
            if (v >= 100)     { pct[0] = '1'; pct[1] = '0'; pct[2] = '0'; pct[3] = 0; }
            else if (v >= 10) { pct[0] = (char)('0' + v / 10); pct[1] = (char)('0' + v % 10); pct[2] = 0; }
            else              { pct[0] = (char)('0' + v); pct[1] = 0; }

            SmallTextRenderer::renderText(pct, txt, w, h, PINK);
            Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 3);

            // The track has to be visible even at 0%. BrightnessModel can get
            // away with an almost-black (14,14,16) track because its bar is
            // never empty; here 0% is a real state, and with an invisible track
            // the screen was a single lone digit — indistinguishable from a
            // hung device at exactly the moment you are watching for progress.
            Draw::rect(buf, TRACK_X, BAR_Y, TRACK_W, BAR_H, 30, 30, 38);

            // And always show at least one lit pixel once the write has started,
            // so the bar reads as "begun" rather than "not responding".
            uint8_t fill = (uint8_t)(((uint16_t)TRACK_W * otaPct) / 100);
            if (fill < 1)       fill = 1;
            if (fill > TRACK_W) fill = TRACK_W;
            Draw::rect(buf, TRACK_X, BAR_Y, fill, BAR_H, PINK[0], PINK[1], PINK[2]);
            break;
        }

        case OtaPhase::SUCCEEDED:
            SmallTextRenderer::renderText("OK", txt, w, h, GREEN);
            Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 4);
            Draw::rect(buf, TRACK_X, BAR_Y, TRACK_W, BAR_H, GREEN[0], GREEN[1], GREEN[2]);
            break;

        case OtaPhase::FAILED:
            SmallTextRenderer::renderText("ERR", txt, w, h, RED);
            Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 4);
            Draw::rect(buf, TRACK_X, BAR_Y, TRACK_W, BAR_H, RED[0], RED[1], RED[2]);
            break;

        case OtaPhase::IDLE:
        default:
            SmallTextRenderer::renderText("OTA", txt, w, h, PINK);
            Draw::blit(buf, txt, w, h, (int16_t)((16 - (int16_t)w) / 2), 6);
            break;
    }
}

void UiController::render(uint8_t* buf, uint32_t nowMs) {
    switch (cur) {
        case Screen::GAMES_MENU:
            gamesMenu.render(buf, nowMs);
            break;

        case Screen::GAME_PLAYING:
            memcpy(buf, engine.buffer(), Draw::SIZE);
            break;

        case Screen::SETTINGS_MENU:
            settingsMenu.render(buf, nowMs);
            break;

        case Screen::BRIGHTNESS_EDIT:
            BrightnessModel::renderScreen(buf, brightness);
            break;

        case Screen::PLACES_MENU:
            placesMenu.render(buf, nowMs);
            break;

        case Screen::UNITS_EDIT:
            renderUnits(buf);
            break;

        case Screen::IP_VIEW:
            renderIp(buf, nowMs);
            break;

        case Screen::UPDATE:
            renderUpdate(buf);
            break;

        case Screen::WEATHER:
            WeatherView::render(buf, weather, tempUnit,
                                Places::get(place).code,
                                (uint32_t)(nowMs - weatherEnteredMs));
            break;

        default:
            Draw::clear(buf);
            break;
    }
}
