#include "Canvas.h"
#include "Transformer.h"
#include "MarqueeEngine.h"
#include "UiController.h"
#include "Draw.h"
#include "WeatherView.h"
#include <emscripten/bind.h>
#include <string.h>

using namespace emscripten;

// Wrapper for the Canvas to expose pixel data
class CanvasWrapper {
public:
    Canvas canvas;
    MarqueeEngine marquee;
    uint8_t marqueeBuffer[4096];
    uint8_t marqueeW, marqueeH, marqueeRot, marqueeY;

    void drawSprite(uintptr_t dataPtr, uint8_t w, uint8_t h, int8_t x, int8_t y, uint8_t rotation, bool clearBefore) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(dataPtr);
        canvas.drawSprite(data, w, h, x, y, rotation, clearBefore);
    }

    void startMarquee(uintptr_t dataPtr, uint8_t w, uint8_t h, uint8_t rotation, int8_t y, uint32_t currentTimeMs) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(dataPtr);
        size_t size = w * h * 3;
        if (size > 4096) size = 4096;
        memcpy(marqueeBuffer, data, size);
        marqueeW = w; marqueeH = h; marqueeRot = rotation; marqueeY = y;
        marquee.start(marqueeBuffer, w, h, 20, currentTimeMs);
    }

    void updateMarquee(uint32_t currentTimeMs) {
        if (marquee.isActive()) {
            int16_t xOff = marquee.getXOffset(currentTimeMs);
            canvas.drawSprite(marqueeBuffer, marqueeW, marqueeH, xOff, marqueeY, marqueeRot, true);
        }
    }

    void stopMarquee() {
        marquee.stop();
    }

    bool isMarqueeActive() {
        return marquee.isActive();
    }

    uintptr_t getBuffer() {
        return reinterpret_cast<uintptr_t>(canvas.getBuffer());
    }

    void clear() {
        canvas.clear();
    }
};

// ── DeviceUI ──────────────────────────────────────────────────────────────────
//
// Exposes the real UiController to the browser so the simulator drives exactly
// the state machine the ESP32 runs (docs/adr/0009), rather than a JS
// reimplementation that would silently drift from the firmware.
//
// The browser supplies `nowMs`, which is the same injection the unit tests use.
class DeviceUIWrapper {
public:
    UiController ui;
    uint8_t      frame[Draw::SIZE];

    DeviceUIWrapper() { memset(frame, 0, sizeof(frame)); }

    void enterGames(uint32_t nowMs)    { ui.enterGames(nowMs); }
    void enterSettings(uint32_t nowMs) { ui.enterSettings(nowMs); }
    void enterWeather(uint32_t nowMs)  { ui.enterWeather(nowMs); }

    void turn(int delta, uint32_t nowMs) { ui.turn(delta, nowMs); }
    void press(uint32_t nowMs)           { ui.press(nowMs); }
    bool longPress(uint32_t nowMs)       { return ui.longPress(nowMs); }

    // Advance and render in one call — the browser's animation loop wants both.
    void tick(uint32_t nowMs) {
        ui.update(nowMs);
        ui.render(frame, nowMs);
    }

    uintptr_t getBuffer() { return reinterpret_cast<uintptr_t>(frame); }

    int screen()          { return (int)ui.screen(); }
    int brightnessLevel() { return ui.brightnessLevel(); }
    int placeIndex()      { return ui.placeIndex(); }
    int unit()            { return (int)ui.unit(); }
    int currentGame()     { return (int)ui.currentGame(); }
    bool cycling()        { return ui.cycling(); }

    void setBrightnessLevel(int level) { ui.setBrightnessLevel((uint8_t)level); }
    void setPlaceIndex(int index)      { ui.setPlaceIndex((uint8_t)index); }
    void setUnit(int u) {
        ui.setUnit(u ? TempUnit::FAHRENHEIT : TempUnit::CELSIUS);
    }

    // OTA. The browser has no flash to write, so it stands in for the firmware:
    // otaRequested() goes true when the user confirms, and the page (or the node
    // harness) drives the progress and the verdict back in. That means the whole
    // update flow except the HTTP fetch can be exercised without a device.
    bool otaRequested()  { return ui.otaRequested(); }
    void clearOtaRequest() { ui.clearOtaRequest(); }
    int  otaPhase()      { return (int)ui.otaPhase(); }
    int  otaProgress()   { return ui.otaProgress(); }
    void setOtaProgress(int pct) { ui.setOtaProgress((uint8_t)pct); }
    void setOtaResult(bool ok)   { ui.setOtaResult(ok); }

    // Stands in for WeatherClient, which only exists on the firmware side.
    void setWeather(int tempC10, int wmoCode, bool isDay, bool valid) {
        WeatherData d;
        d.tempC10 = (int16_t)tempC10;
        d.wmoCode = (uint8_t)wmoCode;
        d.isDay   = isDay;
        d.valid   = valid;
        ui.setWeather(d);
    }
};

EMSCRIPTEN_BINDINGS(leddite_module) {
    class_<DeviceUIWrapper>("DeviceUI")
        .constructor<>()
        .function("enterGames",          &DeviceUIWrapper::enterGames)
        .function("enterSettings",       &DeviceUIWrapper::enterSettings)
        .function("enterWeather",        &DeviceUIWrapper::enterWeather)
        .function("turn",                &DeviceUIWrapper::turn)
        .function("press",               &DeviceUIWrapper::press)
        .function("longPress",           &DeviceUIWrapper::longPress)
        .function("tick",                &DeviceUIWrapper::tick)
        .function("getBuffer",           &DeviceUIWrapper::getBuffer)
        .function("screen",              &DeviceUIWrapper::screen)
        .function("brightnessLevel",     &DeviceUIWrapper::brightnessLevel)
        .function("placeIndex",          &DeviceUIWrapper::placeIndex)
        .function("unit",                &DeviceUIWrapper::unit)
        .function("currentGame",         &DeviceUIWrapper::currentGame)
        .function("cycling",             &DeviceUIWrapper::cycling)
        .function("setBrightnessLevel",  &DeviceUIWrapper::setBrightnessLevel)
        .function("setPlaceIndex",       &DeviceUIWrapper::setPlaceIndex)
        .function("setUnit",             &DeviceUIWrapper::setUnit)
        .function("setWeather",          &DeviceUIWrapper::setWeather)
        .function("otaRequested",        &DeviceUIWrapper::otaRequested)
        .function("clearOtaRequest",     &DeviceUIWrapper::clearOtaRequest)
        .function("otaPhase",            &DeviceUIWrapper::otaPhase)
        .function("otaProgress",         &DeviceUIWrapper::otaProgress)
        .function("setOtaProgress",      &DeviceUIWrapper::setOtaProgress)
        .function("setOtaResult",        &DeviceUIWrapper::setOtaResult);

    class_<CanvasWrapper>("Canvas")
        .constructor<>()
        .function("drawSprite", &CanvasWrapper::drawSprite)
        .function("startMarquee", &CanvasWrapper::startMarquee)
        .function("updateMarquee", &CanvasWrapper::updateMarquee)
        .function("stopMarquee", &CanvasWrapper::stopMarquee)
        .function("isMarqueeActive", &CanvasWrapper::isMarqueeActive)
        .function("getBuffer", &CanvasWrapper::getBuffer)
        .function("clear", &CanvasWrapper::clear);
}
