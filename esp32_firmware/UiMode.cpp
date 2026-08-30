#include "UiMode.h"
#include "BrightnessModel.h"
#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>

// NVS namespace and keys. Keys are <=15 chars (an NVS limit).
static const char* NVS_NS     = "leddite";
static const char* KEY_BRIGHT = "bright";
static const char* KEY_PLACE  = "place";
static const char* KEY_UNIT   = "unit";

void UiMode::begin() {
    prefs.begin(NVS_NS, /*readOnly=*/false);

    // Defaults match the controller's own, so a fresh device behaves sensibly.
    const uint8_t level = prefs.getUChar(KEY_BRIGHT, BrightnessModel::DEFAULT_LEVEL);
    const uint8_t place = prefs.getUChar(KEY_PLACE,  Places::DEFAULT_INDEX);
    const uint8_t unit  = prefs.getUChar(KEY_UNIT,   (uint8_t)TempUnit::CELSIUS);

    // The setters clamp, so a corrupt or out-of-date NVS value cannot put the
    // controller into an invalid state.
    ui.setBrightnessLevel(level);
    ui.setPlaceIndex(place);
    ui.setUnit(unit ? TempUnit::FAHRENHEIT : TempUnit::CELSIUS);
    ui.clearSettingsDirty();   // loading is not a change

    applyBrightness();

    Serial.printf("[UI] settings: brightness=%u place=%s unit=%c\n",
                  ui.brightnessLevel(),
                  Places::get(ui.placeIndex()).code,
                  ui.unit() == TempUnit::FAHRENHEIT ? 'F' : 'C');
}

void UiMode::applyBrightness() {
    const uint8_t level = ui.brightnessLevel();
    if (level == appliedLevel) return;

    appliedLevel = level;
    FastLED.setBrightness(BrightnessModel::levelToFastLED(level));
    Serial.printf("[UI] brightness level %u -> FastLED %u (worst case %lu mA)\n",
                  level, BrightnessModel::levelToFastLED(level),
                  (unsigned long)BrightnessModel::worstCaseMilliamps(level));
}

void UiMode::persistIfDirty() {
    if (!ui.settingsDirty()) return;

    prefs.putUChar(KEY_BRIGHT, ui.brightnessLevel());
    prefs.putUChar(KEY_PLACE,  ui.placeIndex());
    prefs.putUChar(KEY_UNIT,   (uint8_t)ui.unit());
    ui.clearSettingsDirty();

    Serial.printf("[UI] saved: brightness=%u place=%s unit=%c\n",
                  ui.brightnessLevel(),
                  Places::get(ui.placeIndex()).code,
                  ui.unit() == TempUnit::FAHRENHEIT ? 'F' : 'C');
}

void UiMode::refreshNetworkStatus() {
    if (WiFi.status() != WL_CONNECTED) { ui.setNetworkDown(); return; }
    const IPAddress a = WiFi.localIP();
    ui.setIpAddress(a[0], a[1], a[2], a[3]);
}

// ── Entry points ──────────────────────────────────────────────────────────────

void UiMode::enterGames(Canvas& canvas) {
    ui.enterGames(millis());
    lastFrameMs = 0;      // force an immediate first frame
    update(canvas);
}

void UiMode::enterSettings(Canvas& canvas) {
    ui.enterSettings(millis());
    lastFrameMs = 0;
    update(canvas);
}

void UiMode::enterWeather(Canvas& canvas) {
    ui.enterWeather(millis());
    lastFrameMs = 0;
    update(canvas);
}

// ── Encoder ───────────────────────────────────────────────────────────────────

void UiMode::onEncoderTurn(int delta) {
    ui.turn(delta, millis());
    // Brightness is applied live so the panel itself previews the level.
    applyBrightness();
    lastFrameMs = 0;
}

void UiMode::onEncoderPress() {
    ui.press(millis());
    applyBrightness();
    persistIfDirty();     // confirming a value is the natural point to save
    lastFrameMs = 0;
}

bool UiMode::onLongPress() {
    const bool exitToMenu = ui.longPress(millis());
    applyBrightness();
    persistIfDirty();     // backing out keeps the change, so save it too
    lastFrameMs = 0;
    return exitToMenu;
}

// ── Frame ─────────────────────────────────────────────────────────────────────

void UiMode::update(Canvas& canvas) {
    const uint32_t now = millis();
    if (lastFrameMs != 0 && (uint32_t)(now - lastFrameMs) < FRAME_MS) return;
    lastFrameMs = now;

    ui.update(now);
    ui.render(pixBuf, now);

    canvas.drawSprite(pixBuf, 16, 16, 0, 0, 0, /*clearBefore=*/true);
}
