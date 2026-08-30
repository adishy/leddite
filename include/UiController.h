#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

// UiController — the navigation state machine for the Games and Settings trees.
//
// Pure C++/stdint (docs/adr/0009): no Arduino, no FastLED, no millis(). Both the
// ESP32 wrappers (GameMode, SettingsMode) and the WASM simulator binding drive
// *this* object, so the simulator exercises exactly the code the device runs
// rather than a reimplementation that can drift.
//
// SCREENS
//   GAMES_MENU      list of games (Game enum order) + "CYCLE ALL"
//   GAME_PLAYING    a game running full-screen
//   SETTINGS_MENU   BRIGHTNESS / PLACE / UNITS / IP / UPDATE
//   BRIGHTNESS_EDIT level 1-10
//   PLACES_MENU     the Places table
//   UNITS_EDIT      Celsius or Fahrenheit
//   IP_VIEW         the device's own address, scrolling in the 5x7 font
//   UPDATE          OTA: confirm, then progress, then a result
//   WEATHER         icon + temperature for the selected place
//
// INPUT CONTRACT
//   turn(delta)  move selection / adjust value
//   press()      select / confirm
//   longPress()  go up one level; returns true when the caller should return to
//                the device's own main menu (i.e. we were already at a root
//                screen and have nowhere further up to go)
//
// Persisted values (brightness level, place index, unit) are exposed as plain
// getters/setters so the firmware can mirror them into NVS without this class
// knowing that NVS exists.

#include <stdint.h>
#include "ListMenu.h"
#include "GameEngine.h"
#include "WeatherView.h"
#include "Places.h"

class UiController {
public:
    enum class Screen : uint8_t {
        GAMES_MENU = 0,
        GAME_PLAYING,
        SETTINGS_MENU,
        BRIGHTNESS_EDIT,
        PLACES_MENU,
        UNITS_EDIT,
        WEATHER,
        IP_VIEW,
        UPDATE,
    };

    // OTA lifecycle. UiController owns the screen and the request flag; the
    // firmware owns the flash write and reports back through setOtaProgress()
    // and setOtaResult(). Keeping the split here means the whole flow except the
    // HTTP fetch is unit-testable and visible in the simulator (docs/adr/0014).
    enum class OtaPhase : uint8_t {
        IDLE = 0,
        CONFIRM,     // "OTA — YES / NO"
        RUNNING,     // progress bar, driven by setOtaProgress()
        SUCCEEDED,   // the device is about to reboot
        FAILED,      // press or long-press to back out
    };

    // How long each game runs before "CYCLE ALL" advances to the next.
    static const uint32_t CYCLE_INTERVAL_MS = 20000;

    UiController();

    // ── Entry points (called when the device main menu selects a mode) ────────
    void enterGames(uint32_t nowMs);
    void enterSettings(uint32_t nowMs);
    void enterWeather(uint32_t nowMs);

    // ── Input ────────────────────────────────────────────────────────────────
    void turn(int delta, uint32_t nowMs);
    void press(uint32_t nowMs);
    bool longPress(uint32_t nowMs);   // true => hand control back to the main menu

    // ── Frame ────────────────────────────────────────────────────────────────
    void update(uint32_t nowMs);          // advances games / cycling
    void render(uint8_t* buf, uint32_t nowMs);

    // ── State (the firmware mirrors these into NVS) ──────────────────────────
    Screen   screen()         const { return cur; }
    uint8_t  brightnessLevel() const { return brightness; }
    uint8_t  placeIndex()      const { return place; }
    TempUnit unit()            const { return tempUnit; }

    void setBrightnessLevel(uint8_t level);
    void setPlaceIndex(uint8_t index);
    void setUnit(TempUnit u);

    // True when a setting changed since the flag was last cleared — lets the
    // firmware write NVS only when something actually moved.
    bool  settingsDirty() const { return dirty; }
    void  clearSettingsDirty()  { dirty = false; }

    // ── Network identity, supplied by the firmware ───────────────────────────
    // You cannot OTA a device whose address you do not know, and the address is
    // otherwise only ever printed to a serial console nobody has attached. The
    // controller stores four octets rather than a string so it stays free of
    // any allocation and of Arduino's IPAddress type (docs/adr/0009).
    void setIpAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
    void setNetworkDown();
    bool hasIpAddress() const { return ipValid; }

    // ── Weather data, supplied by the firmware's WeatherClient ───────────────
    void setWeather(const WeatherData& d) { weather = d; }
    const WeatherData& weatherData() const { return weather; }

    // Which game is running (or would run) — exposed for tests and logging.
    Game currentGame() const { return engine.game(); }
    bool cycling()     const { return cycleAll; }

    // ── OTA ──────────────────────────────────────────────────────────────────
    // The firmware polls otaRequested() every loop; when it is set it clears the
    // request and performs the update, calling setOtaProgress() as bytes land
    // and setOtaResult() at the end. Nothing here knows what HTTP is.
    bool     otaRequested()  const { return otaReq; }
    void     clearOtaRequest()     { otaReq = false; }
    OtaPhase otaPhase()      const { return otaSt; }
    uint8_t  otaProgress()   const { return otaPct; }
    void     setOtaProgress(uint8_t pct);
    void     setOtaResult(bool ok);

private:
    void startGame(uint8_t gameIndex, uint32_t nowMs);
    void buildPlacesMenu();
    void renderUnits(uint8_t* buf) const;
    void renderUpdate(uint8_t* buf) const;
    void renderIp(uint8_t* buf, uint32_t nowMs) const;
    // Draws `text` in the 5x7 font one glyph at a time at (x, y). Per-glyph
    // avoids staging the whole 90px string in a 2KB buffer just to blit it.
    static void drawWide(uint8_t* buf, const char* text, int16_t x, int16_t y,
                         const uint8_t* colour);
    int16_t ipScrollOffset(uint16_t textW, uint32_t nowMs) const;

    Screen   cur       = Screen::GAMES_MENU;
    uint8_t  brightness = 4;                       // BrightnessModel::DEFAULT_LEVEL
    uint8_t  place      = Places::DEFAULT_INDEX;
    TempUnit tempUnit   = TempUnit::CELSIUS;
    bool     dirty      = false;

    ListMenu   gamesMenu;
    ListMenu   settingsMenu;
    ListMenu   placesMenu;
    GameEngine engine;

    // Built at construction from the Places table so both stay in step.
    MenuItem   placeItems[Places::COUNT];

    bool     cycleAll     = false;
    uint8_t  cycleIndex   = 0;
    uint32_t cycleStartMs = 0;

    WeatherData weather;
    uint32_t    weatherEnteredMs = 0;

    // Scroll pacing for the address. Slower than a menu label: this is a string
    // you are copying down digit by digit, not one you are skimming.
    static const uint16_t IP_DWELL_MS = 900;
    static const uint8_t  IP_PPS      = 17;   // px/sec
    static const uint8_t  IP_GAP_PX   = 8;    // blank gap between wrap repeats

    uint8_t  ip[4]  = { 0, 0, 0, 0 };
    bool     ipValid = false;
    uint32_t ipEnteredMs = 0;

    OtaPhase otaSt  = OtaPhase::IDLE;
    bool     otaYes = false;    // CONFIRM defaults to NO — this reflashes the device
    bool     otaReq = false;
    uint8_t  otaPct = 0;
};

#endif // UI_CONTROLLER_H
