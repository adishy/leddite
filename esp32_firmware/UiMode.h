#pragma once

#include "Canvas.h"
#include "UiController.h"
#include <Preferences.h>
#include <stdint.h>

// UiMode — the Arduino shell around UiController.
//
// UiController holds all the navigation logic and is Arduino-free so it can be
// unit-tested and run in the browser (docs/adr/0009, 0002). This class supplies
// the three things it deliberately does not know about:
//
//   1. millis()          — the time source
//   2. Canvas            — pushing the rendered buffer to the panel
//   3. Preferences (NVS) — persisting brightness, place and unit across reboots
//
// Deliberately thin. If logic starts accumulating here it belongs in
// UiController instead, where it can be tested.

class UiMode {
public:
    static const uint8_t FRAME_MS = 33;   // ~30 FPS

    // Loads persisted settings from NVS and applies brightness. Call once in setup().
    void begin();

    // Entry points, matching the main menu selections.
    void enterGames(Canvas& canvas);
    void enterSettings(Canvas& canvas);
    void enterWeather(Canvas& canvas);

    // Encoder. onLongPress() returns true when the caller should return to the
    // device main menu — the controller handles going up a level internally.
    void onEncoderTurn(int delta);
    void onEncoderPress();
    bool onLongPress();

    void update(Canvas& canvas);   // call every loop()

    // Weather data arrives from WeatherClient and is forwarded to the controller.
    void setWeather(const WeatherData& d) { ui.setWeather(d); }

    uint8_t  brightnessLevel() const { return ui.brightnessLevel(); }
    uint8_t  placeIndex()      const { return ui.placeIndex(); }
    TempUnit unit()            const { return ui.unit(); }

    UiController& controller() { return ui; }

private:
    void applyBrightness();        // push the level into FastLED
    void persistIfDirty();         // write NVS only when something changed

    UiController ui;
    Preferences  prefs;
    uint8_t      appliedLevel = 0;      // 0 = nothing applied yet
    uint32_t     lastFrameMs  = 0;
    uint8_t      pixBuf[16 * 16 * 3];   // member, not stack — 768 B
};
