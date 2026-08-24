#include "WeatherView.h"
#include "Draw.h"
#include "SmallTextRenderer.h"
#include <stdio.h>
#include <string.h>

// Scratch for one rendered string. Sized in PIXELS, not characters — the render
// buffer is textWidth * CHAR_HEIGHT * 3 bytes, and SmallTextRenderer advances up
// to 4px per character. 32px covers any label this view draws with room to
// spare; every call still guards on textWidth() before rendering.
static const uint16_t TXT_MAX_W = 32;
static uint8_t        txtBuf[TXT_MAX_W * SmallTextRenderer::CHAR_HEIGHT * 3];

// ── WMO code mapping ──────────────────────────────────────────────────────────
//
// Open-Meteo weather_code values:
//   0        clear
//   1-3      mainly clear / partly cloudy / overcast
//   45,48    fog, depositing rime fog
//   51-57    drizzle (56,57 freezing)
//   61-67    rain (66,67 freezing)
//   71-77    snow fall / snow grains
//   80-82    rain showers
//   85,86    snow showers
//   95-99    thunderstorm (96,99 with hail)

WeatherView::Icon WeatherView::iconFor(uint8_t wmoCode, bool isDay) {
    switch (wmoCode) {
        case 0:
        case 1:  return isDay ? CLEAR_DAY  : CLEAR_NIGHT;
        case 2:  return isDay ? PARTLY_DAY : PARTLY_NIGHT;
        case 3:  return CLOUDY;
        case 45:
        case 48: return FOG;
        default: break;
    }

    if (wmoCode >= 51 && wmoCode <= 57) return DRIZZLE;
    if (wmoCode >= 61 && wmoCode <= 67) return RAIN;
    if (wmoCode >= 71 && wmoCode <= 77) return SNOW;
    if (wmoCode >= 80 && wmoCode <= 82) return RAIN;
    if (wmoCode == 85 || wmoCode == 86) return SNOW;
    if (wmoCode >= 95 && wmoCode <= 99) return THUNDER;

    return UNKNOWN;
}

uint8_t WeatherView::precipStreaks(uint8_t wmoCode) {
    switch (wmoCode) {
        // Slight
        case 51: case 56: case 61: case 66: case 71: case 80: case 85: return 2;
        // Heavy / violent
        case 55: case 57: case 65: case 67: case 75: case 82: case 86: return 4;
        default: return 3;   // moderate, and anything unclassified
    }
}

bool WeatherView::isFreezing(uint8_t wmoCode) {
    return wmoCode == 56 || wmoCode == 57 || wmoCode == 66 || wmoCode == 67;
}

// ── Temperature ───────────────────────────────────────────────────────────────

int16_t WeatherView::displayTemp(int16_t tempC10, TempUnit unit) {
    if (unit == TempUnit::FAHRENHEIT) {
        // Convert in tenths first to avoid compounding rounding error.
        const int32_t f10 = ((int32_t)tempC10 * 9) / 5 + 320;
        return (int16_t)((f10 >= 0) ? (f10 + 5) / 10 : (f10 - 5) / 10);
    }
    return (int16_t)((tempC10 >= 0) ? (tempC10 + 5) / 10 : (tempC10 - 5) / 10);
}

void WeatherView::formatTemp(int16_t tempC10, TempUnit unit, char* out, size_t n) {
    if (!out || n == 0) return;
    const int16_t t = displayTemp(tempC10, unit);
    snprintf(out, n, "%d%c", (int)t, unit == TempUnit::FAHRENHEIT ? 'F' : 'C');
}

// ── Primitives ────────────────────────────────────────────────────────────────
//
// Coordinates are in tenths of a pixel so discs can be centred between pixels,
// which matters a lot when a "circle" is only four pixels across.

void WeatherView::disc(uint8_t* buf, int16_t cx10, int16_t cy10, int16_t r10,
                       uint8_t r, uint8_t g, uint8_t b) {
    const int32_t rr = (int32_t)r10 * r10;
    for (int16_t y = 0; y < 10; y++) {
        for (int16_t x = 0; x < 16; x++) {
            const int32_t dx = (int32_t)(x * 10 + 5) - cx10;
            const int32_t dy = (int32_t)(y * 10 + 5) - cy10;
            if (dx * dx + dy * dy <= rr) Draw::px(buf, x, y, r, g, b);
        }
    }
}

void WeatherView::cloud(uint8_t* buf, int16_t yOff, uint8_t r, uint8_t g, uint8_t b) {
    // Three overlapping lobes plus a flat base reads as a cloud at this size.
    disc(buf, 55, (int16_t)(yOff + 45), 22, r, g, b);
    disc(buf, 85, (int16_t)(yOff + 33), 27, r, g, b);
    disc(buf, 112, (int16_t)(yOff + 47), 21, r, g, b);
    Draw::rect(buf, 3, (int16_t)(yOff / 10 + 5), 10, 2, r, g, b);
}

void WeatherView::sun(uint8_t* buf, int16_t cx, int16_t cy, bool rays) {
    disc(buf, (int16_t)(cx * 10 + 5), (int16_t)(cy * 10 + 5), 25, 255, 190, 20);
    disc(buf, (int16_t)(cx * 10 + 5), (int16_t)(cy * 10 + 5), 13, 255, 235, 120);

    if (!rays) return;
    static const int8_t RX[8] = { 0,  0, -4, 4, -3,  3, -3, 3 };
    static const int8_t RY[8] = {-4,  4,  0, 0, -3, -3,  3, 3 };
    for (uint8_t i = 0; i < 8; i++)
        Draw::px(buf, (int16_t)(cx + RX[i]), (int16_t)(cy + RY[i]), 255, 200, 40);
}

void WeatherView::moon(uint8_t* buf, int16_t cx, int16_t cy) {
    // Crescent: a bright disc with a background-coloured disc bitten out of it.
    // The bite is offset up and right by a little over one pixel and is only
    // slightly smaller than the disc — biting harder than this leaves a sliver
    // that reads as noise rather than as a moon at 16x16.
    disc(buf, (int16_t)(cx * 10 + 5), (int16_t)(cy * 10 + 5), 34, 225, 230, 255);
    disc(buf, (int16_t)(cx * 10 + 19), (int16_t)(cy * 10 - 4), 30, 0, 0, 0);
    Draw::px(buf, (int16_t)(cx + 5), (int16_t)(cy - 2), 200, 210, 255);   // star
}

// A narrower cloud for the "partly" icons, so the sun or moon behind it stays
// visible. The full-width cloud() occludes almost the entire 16px row.
void WeatherView::cloudSmall(uint8_t* buf, int16_t yOff,
                             uint8_t r, uint8_t g, uint8_t b) {
    disc(buf, 35, (int16_t)(yOff + 45), 20, r, g, b);
    disc(buf, 62, (int16_t)(yOff + 35), 24, r, g, b);
    disc(buf, 88, (int16_t)(yOff + 46), 19, r, g, b);
    Draw::rect(buf, 1, (int16_t)(yOff / 10 + 5), 9, 2, r, g, b);
}

// ── Icon rendering ────────────────────────────────────────────────────────────

void WeatherView::drawIcon(uint8_t* buf, Icon icon, uint8_t streaks,
                           bool freezing, uint32_t elapsedMs) {
    const uint8_t CR = 168, CG = 178, CB = 196;          // cloud body
    const uint8_t DR = 96,  DG = 104, DB = 122;          // shaded cloud

    // Precipitation scrolls downward; the phase also drives the lightning flash.
    const uint8_t phase = (uint8_t)((elapsedMs / 160u) % 3u);

    switch (icon) {
        case CLEAR_DAY:
            sun(buf, 7, 4, true);
            break;

        case CLEAR_NIGHT:
            moon(buf, 7, 4);
            break;

        case PARTLY_DAY:
            // Sun tucked into the top-right, cloud drawn after so it overlaps.
            sun(buf, 12, 2, false);
            cloudSmall(buf, 14, CR, CG, CB);
            break;

        case PARTLY_NIGHT:
            moon(buf, 12, 2);
            cloudSmall(buf, 14, CR, CG, CB);
            break;

        case CLOUDY:
            cloud(buf, 16, DR, DG, DB);
            cloud(buf, 4, CR, CG, CB);
            break;

        case FOG:
            // Offset bars, drifting sideways, read as fog better than a cloud.
            for (uint8_t i = 0; i < 4; i++) {
                const int16_t y = (int16_t)(2 + i * 2);
                const int16_t x = (int16_t)(((i % 2) ? 1 : 3) +
                                            ((elapsedMs / 400u + i) % 3u));
                Draw::rect(buf, x, y, 11, 1, CR, CG, CB);
            }
            break;

        case DRIZZLE:
        case RAIN: {
            cloud(buf, 0, CR, CG, CB);
            const uint8_t pr = freezing ? 190 : 90;
            const uint8_t pg = freezing ? 240 : 170;
            const uint8_t pb = 255;
            const uint8_t len = (icon == RAIN) ? 2 : 1;
            for (uint8_t s = 0; s < streaks; s++) {
                const int16_t x = (int16_t)(3 + s * 3);
                const int16_t y = (int16_t)(7 + ((phase + s) % 3));
                for (uint8_t k = 0; k < len; k++)
                    Draw::px(buf, x, (int16_t)(y + k), pr, pg, pb);
            }
            break;
        }

        case SNOW: {
            cloud(buf, 0, CR, CG, CB);
            for (uint8_t s = 0; s < streaks; s++) {
                const int16_t x = (int16_t)(3 + s * 3);
                const int16_t y = (int16_t)(7 + ((phase + s) % 3));
                Draw::px(buf, x, y, 235, 245, 255);
            }
            break;
        }

        case THUNDER: {
            cloud(buf, 0, DR, DG, DB);
            // Bolt flashes rather than sitting static.
            const bool flash = (phase != 2);
            const uint8_t br = flash ? 255 : 150;
            const uint8_t bg = flash ? 225 : 120;
            const uint8_t bb = flash ? 60  : 30;
            Draw::px(buf, 8, 6, br, bg, bb);
            Draw::px(buf, 7, 7, br, bg, bb);
            Draw::px(buf, 8, 7, br, bg, bb);
            Draw::px(buf, 6, 8, br, bg, bb);
            Draw::px(buf, 7, 8, br, bg, bb);
            Draw::px(buf, 7, 9, br, bg, bb);
            break;
        }

        case UNKNOWN:
        default: {
            // Doubles as the fetch-failure state: a stale reading must never be
            // presented as if it were live.
            const uint8_t q[3] = { 120, 120, 130 };
            uint16_t w = 0, h = 0;
            SmallTextRenderer::renderText("-", txtBuf, w, h, q);
            Draw::blit(buf, txtBuf, w, h, 6, 4);
            Draw::rect(buf, 4, 2, 8, 1, q[0], q[1], q[2]);
            Draw::rect(buf, 4, 7, 8, 1, q[0], q[1], q[2]);
            break;
        }
    }
}

// ── Full view ─────────────────────────────────────────────────────────────────

void WeatherView::render(uint8_t* buf, const WeatherData& d, TempUnit unit,
                         const char* placeCode, uint32_t elapsedMs) {
    Draw::clear(buf);

    const Icon icon = d.valid ? iconFor(d.wmoCode, d.isDay) : UNKNOWN;

    // Place code takes over the whole panel briefly on entry, so it is always
    // unambiguous which location is being shown.
    if (placeCode && elapsedMs < PLACE_FLASH_MS) {
        const uint8_t c[3] = { 120, 200, 255 };
        uint16_t w = 0, h = 0;
        if (SmallTextRenderer::textWidth(placeCode) <= TXT_MAX_W) {
            SmallTextRenderer::renderText(placeCode, txtBuf, w, h, c);
            Draw::blit(buf, txtBuf, w, h, (int16_t)((16 - (int16_t)w) / 2), 6);
        }
        return;
    }

    drawIcon(buf, icon, precipStreaks(d.wmoCode), isFreezing(d.wmoCode), elapsedMs);

    // ── Temperature ───────────────────────────────────────────────────────────
    char tmp[8];
    if (d.valid) formatTemp(d.tempC10, unit, tmp, sizeof(tmp));
    else         snprintf(tmp, sizeof(tmp), "--%c", unit == TempUnit::FAHRENHEIT ? 'F' : 'C');

    const uint8_t tc[3] = { 255, 240, 210 };
    uint16_t w = 0, h = 0;
    if (SmallTextRenderer::textWidth(tmp) <= TXT_MAX_W) {
        SmallTextRenderer::renderText(tmp, txtBuf, w, h, tc);
        Draw::blit(buf, txtBuf, w, h, (int16_t)((16 - (int16_t)w) / 2), 11);
    }
}
