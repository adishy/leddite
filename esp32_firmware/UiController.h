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
//   GAMES_MENU      list of games + "CYCLE ALL"
//   GAME_PLAYING    a game running full-screen
//   SETTINGS_MENU   BRIGHTNESS / PLACE / UNITS
//   BRIGHTNESS_EDIT level 1-10
//   PLACES_MENU     the Places table
//   UNITS_EDIT      Celsius or Fahrenheit
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

    // ── Weather data, supplied by the firmware's WeatherClient ───────────────
    void setWeather(const WeatherData& d) { weather = d; }
    const WeatherData& weatherData() const { return weather; }

    // Which game is running (or would run) — exposed for tests and logging.
    Game currentGame() const { return engine.game(); }
    bool cycling()     const { return cycleAll; }

private:
    void startGame(uint8_t gameIndex, uint32_t nowMs);
    void buildPlacesMenu();
    void renderUnits(uint8_t* buf) const;

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
};

#endif // UI_CONTROLLER_H
