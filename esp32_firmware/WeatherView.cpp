#include "WeatherView.h"
#include "Draw.h"
#include "SmallTextRenderer.h"
#include "TextRenderer.h"
#include <stdio.h>
#include <string.h>

// Scratch for the small-font strings (place code, temperature). Sized in PIXELS,
// not characters — the render buffer is textWidth * CHAR_HEIGHT * 3 bytes, and
// SmallTextRenderer advances up to 4px per character. Every call still guards on
// textWidth() before rendering.
static const uint16_t TXT_MAX_W = 32;
static uint8_t        txtBuf[TXT_MAX_W * SmallTextRenderer::CHAR_HEIGHT * 3];

// Scratch for the large-font description. TextRenderer's stride is a fixed 6px
// per character, so DESC_MAX_CHARS bounds this exactly.
static const uint16_t DESC_MAX_W = WeatherView::DESC_MAX_CHARS * TextRenderer::CHAR_STRIDE;
static uint8_t        descBuf[DESC_MAX_W * TextRenderer::CHAR_HEIGHT * 3];

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
//
// Wording is uppercase and uses only glyphs TextRenderer has (A-Z, 0-9, space
// and a little punctuation); anything outside that renders as a space. Intensity
// variants collapse where the distinction would not survive being read off a
// scrolling 16px strip — "heavy freezing drizzle" is just "FREEZING DRIZZLE".

const char* WeatherView::describe(uint8_t wmoCode, bool isDay) {
    switch (wmoCode) {
        case 0:  return isDay ? "CLEAR SKY" : "CLEAR NIGHT";
        case 1:  return "MAINLY CLEAR";
        case 2:  return "PARTLY CLOUDY";
        case 3:  return "OVERCAST";

        case 45: return "FOG";
        case 48: return "FREEZING FOG";

        case 51: return "LIGHT DRIZZLE";
        case 53: return "DRIZZLE";
        case 55: return "HEAVY DRIZZLE";
        case 56:
        case 57: return "FREEZING DRIZZLE";

        case 61: return "LIGHT RAIN";
        case 63: return "RAIN";
        case 65: return "HEAVY RAIN";
        case 66:
        case 67: return "FREEZING RAIN";

        case 71: return "LIGHT SNOW";
        case 73: return "SNOW";
        case 75: return "HEAVY SNOW";
        case 77: return "SNOW GRAINS";

        case 80: return "LIGHT SHOWERS";
        case 81: return "RAIN SHOWERS";
        case 82: return "HEAVY SHOWERS";

        case 85: return "LIGHT SNOW SHOWERS";
        case 86: return "SNOW SHOWERS";

        case 95: return "THUNDERSTORM";
        case 96: return "THUNDER AND HAIL";
        case 99: return "SEVERE THUNDERSTORM";

        default: return "UNKNOWN";
    }
}

void WeatherView::conditionColor(uint8_t wmoCode, bool isDay,
                                 uint8_t& r, uint8_t& g, uint8_t& b) {
    // Warm for sun, cool blue as precipitation gets heavier, near-white for
    // snow, flat grey for fog. Freezing codes borrow the icy cyan so a freezing
    // reading never looks like ordinary rain.
    switch (wmoCode) {
        case 0:
            if (isDay) { r = 255; g = 190; b =  40; }   // sun
            else       { r = 150; g = 175; b = 255; }   // moonlight
            return;
        case 1:
            if (isDay) { r = 255; g = 210; b = 110; }
            else       { r = 165; g = 185; b = 240; }
            return;
        case 2:
            if (isDay) { r = 230; g = 205; b = 150; }
            else       { r = 160; g = 175; b = 215; }
            return;
        case 3:  r = 165; g = 175; b = 195; return;     // overcast

        case 45: r = 150; g = 160; b = 170; return;     // fog
        case 48: r = 165; g = 205; b = 225; return;     // freezing fog

        case 71: case 73: case 75: case 77:
        case 85: case 86:
            r = 225; g = 240; b = 255; return;          // snow

        case 95: case 96: case 99:
            r = 255; g = 205; b =  70; return;          // thunder

        default: break;
    }

    if (isFreezing(wmoCode)) { r = 160; g = 235; b = 255; return; }

    if (wmoCode >= 51 && wmoCode <= 57) { r = 110; g = 190; b = 240; return; }  // drizzle
    if (wmoCode >= 61 && wmoCode <= 67) { r =  80; g = 150; b = 255; return; }  // rain
    if (wmoCode >= 80 && wmoCode <= 82) { r =  70; g = 165; b = 245; return; }  // showers

    r = 120; g = 120; b = 130;                                                 // unknown
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
    const char    u = (unit == TempUnit::FAHRENHEIT) ? 'F' : 'C';

    snprintf(out, n, "%d%c%c", (int)t, SmallTextRenderer::DEGREE_CHAR, u);

    // "-12*C" and "212*F" are 18px in a 16px row. The temperature row has no
    // scroll fallback by design, so shed the unit letter — which the user chose
    // and which the rest of the UI already shows — before the degree mark.
    if (SmallTextRenderer::textWidth(out) > Draw::W)
        snprintf(out, n, "%d%c", (int)t, SmallTextRenderer::DEGREE_CHAR);
}

// ── Description scrolling ─────────────────────────────────────────────────────

int16_t WeatherView::descScrollOffset(uint16_t descW, uint32_t elapsedMs) {
    if (descW <= Draw::W) return 0;             // fits: never scrolls

    // Scrolling is timed from the moment the description takes over the panel,
    // not from view entry, so the dwell is not eaten by the place-code flash.
    // Callers may pass no place code, in which case there is no flash and
    // elapsedMs can be below PLACE_FLASH_MS — clamp rather than underflow.
    const uint32_t since = (elapsedMs > PLACE_FLASH_MS)
                         ? (elapsedMs - PLACE_FLASH_MS) : 0u;
    if (since <= DESC_DWELL_MS) return 0;

    const uint32_t travel = (uint32_t)descW + DESC_GAP_PX;
    const uint32_t moved  = ((since - DESC_DWELL_MS) * DESC_PPS) / 1000u;

    return (int16_t)-(int16_t)(moved % travel);
}

// ── Full view ─────────────────────────────────────────────────────────────────

void WeatherView::render(uint8_t* buf, const WeatherData& d, TempUnit unit,
                         const char* placeCode, uint32_t elapsedMs) {
    Draw::clear(buf);

    // Place code takes over the whole panel briefly on entry, so it is always
    // unambiguous which location is being shown.
    if (placeCode && elapsedMs < PLACE_FLASH_MS) {
        const uint8_t c[3] = { 120, 200, 255 };
        uint16_t w = 0, h = 0;
        if (SmallTextRenderer::textWidth(placeCode) <= TXT_MAX_W) {
            SmallTextRenderer::renderText(placeCode, txtBuf, w, h, c);
            Draw::blit(buf, txtBuf, w, h, (int16_t)((Draw::W - (int16_t)w) / 2), 6);
        }
        return;
    }

    // A failed or absent fetch must never be presented as a real reading, so it
    // gets its own wording and the neutral unknown colour rather than defaulting
    // to code 0 (which is "clear sky").
    uint8_t cr, cg, cb;
    const char* desc;
    if (d.valid) {
        conditionColor(d.wmoCode, d.isDay, cr, cg, cb);
        desc = describe(d.wmoCode, d.isDay);
    } else {
        cr = 120; cg = 120; cb = 130;
        desc = "NO DATA";
    }

    // ── Temperature ───────────────────────────────────────────────────────────
    // Lifted toward white so it reads as the headline while still carrying the
    // condition's hue.
    const uint8_t tc[3] = {
        (uint8_t)(cr + ((255 - cr) * 2) / 5),
        (uint8_t)(cg + ((255 - cg) * 2) / 5),
        (uint8_t)(cb + ((255 - cb) * 2) / 5),
    };

    char tmp[10];
    if (d.valid) {
        formatTemp(d.tempC10, unit, tmp, sizeof(tmp));
    } else {
        snprintf(tmp, sizeof(tmp), "--%c%c", SmallTextRenderer::DEGREE_CHAR,
                 unit == TempUnit::FAHRENHEIT ? 'F' : 'C');
    }

    uint16_t w = 0, h = 0;
    if (SmallTextRenderer::textWidth(tmp) <= TXT_MAX_W) {
        SmallTextRenderer::renderText(tmp, txtBuf, w, h, tc);
        Draw::blit(buf, txtBuf, w, h, (int16_t)((Draw::W - (int16_t)w) / 2), TEMP_Y);
    }

    // ── Rule ──────────────────────────────────────────────────────────────────
    Draw::rect(buf, 0, RULE_Y, Draw::W, 1,
               Draw::dim(cr, 110), Draw::dim(cg, 110), Draw::dim(cb, 110));

    // ── Description ───────────────────────────────────────────────────────────
    if (TextRenderer::textWidth(desc) > DESC_MAX_W) return;   // guarded by a test

    const uint8_t dc[3] = { cr, cg, cb };
    uint16_t dw = 0, dh = 0;
    TextRenderer::renderText(desc, descBuf, dw, dh, dc);
    if (dw == 0) return;

    const int16_t xOff = (dw <= Draw::W)
                       ? (int16_t)((Draw::W - (int16_t)dw) / 2)   // short: centred
                       : descScrollOffset(dw, elapsedMs);

    Draw::blitClipped(buf, descBuf, dw, dh, xOff, DESC_Y,
                      DESC_Y, TextRenderer::CHAR_HEIGHT);

    // Second copy so a wrapping scroll has no blank gap at the seam.
    if (dw > Draw::W) {
        Draw::blitClipped(buf, descBuf, dw, dh,
                          (int16_t)(xOff + dw + DESC_GAP_PX), DESC_Y,
                          DESC_Y, TextRenderer::CHAR_HEIGHT);
    }
}
