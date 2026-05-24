// Ground-truth solid-fill firmware.
// Uses the EXACT mapping functions copy-pasted from the proven legacy firmware
// (esp32_visual_timer/led_rotary_enc_visual_timer.ino) with zero modifications.
// No Canvas, no encoder, no WiFi — purely FastLED with the legacy addressing.
//
// Phase A: fill_solid() straight into leds[] — bypasses ALL custom mapping
// Phase B: setPixel_Final() loop — uses the legacy proven mapping
// If A and B match, the mapping is correct.

#include <FastLED.h>

#define LED_DATA_PIN    4
#define LED_TYPE        WS2812B
#define COLOR_ORDER     GRB
#define NUM_LEDS        256
#define BRIGHTNESS      64
const int PANEL_WIDTH  = 16;
const int PANEL_HEIGHT = 16;

CRGB leds[NUM_LEDS];

// -------------------------------------------------------
// LEGACY MAPPING — copied verbatim from
// esp32_visual_timer/led_rotary_enc_visual_timer.ino
// -------------------------------------------------------
uint16_t XY_Serpentine_Original(uint8_t col, uint8_t row) {
    if (col >= PANEL_WIDTH || row >= PANEL_HEIGHT) {
        return NUM_LEDS;
    }
    uint16_t i;
    if (row % 2 == 0) { // Even rows: L to R
        i = (row * PANEL_WIDTH) + col;
    } else { // Odd rows: R to L
        i = (row * PANEL_WIDTH) + (PANEL_WIDTH - 1 - col);
    }
    return i;
}

void setPixel_Final(int final_canvas_x, int final_canvas_y, CRGB color) {
    if (final_canvas_x < 0 || final_canvas_x >= PANEL_WIDTH ||
        final_canvas_y < 0 || final_canvas_y >= PANEL_HEIGHT) {
        return;
    }
    int x_on_180_canvas = (PANEL_HEIGHT - 1) - final_canvas_y;
    int y_on_180_canvas = final_canvas_x;
    uint16_t serpentine_index_for_original_panel =
        XY_Serpentine_Original(x_on_180_canvas, y_on_180_canvas);
    uint16_t physical_led_index = (NUM_LEDS - 1) - serpentine_index_for_original_panel;
    if (physical_led_index < NUM_LEDS) {
        leds[physical_led_index] = color;
    }
}
// -------------------------------------------------------

void legacyFill(CRGB color) {
    for (int y = 0; y < PANEL_HEIGHT; y++) {
        for (int x = 0; x < PANEL_WIDTH; x++) {
            setPixel_Final(x, y, color);
        }
    }
    FastLED.show();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== GROUND TRUTH: Legacy mapping solid fills ===");

    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();
    delay(500);

    // --- Phase A: Raw fill_solid (no custom mapping at all) ---
    // Every LED gets the same color regardless of addressing.
    // Use this to confirm FastLED, pin, and color-order are all correct.
    Serial.println("Phase A: fill_solid RED (raw, no mapping)");
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(2000);

    Serial.println("Phase A: fill_solid GREEN");
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(2000);

    Serial.println("Phase A: fill_solid BLUE");
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(2000);

    FastLED.clear(); FastLED.show();
    delay(500);

    // --- Phase B: setPixel_Final loop (legacy mapping) ---
    // Should look identical to Phase A for solid fills.
    // If it doesn't, the legacy mapping has a bug (unlikely given it passed hardware tests).
    Serial.println("Phase B: legacyFill RED (via setPixel_Final)");
    legacyFill(CRGB::Red);
    delay(2000);

    Serial.println("Phase B: legacyFill GREEN");
    legacyFill(CRGB::Green);
    delay(2000);

    Serial.println("Phase B: legacyFill BLUE");
    legacyFill(CRGB::Blue);
    delay(2000);

    FastLED.clear(); FastLED.show();
    delay(500);

    Serial.println("=== Setup complete, entering loop ===");
}

void loop() {
    // Cycle: A-raw then B-legacy, forever
    static uint8_t phase = 0;
    static const CRGB colors[] = { CRGB::Red, CRGB::Green, CRGB::Blue };
    static const char* colorNames[] = { "RED", "GREEN", "BLUE" };
    uint8_t c = phase % 3;
    bool useRaw = phase < 3;

    if (useRaw) {
        Serial.printf("A-raw %s\n", colorNames[c]);
        fill_solid(leds, NUM_LEDS, colors[c]);
        FastLED.show();
    } else {
        Serial.printf("B-legacy %s\n", colorNames[c]);
        legacyFill(colors[c]);
    }
    phase = (phase + 1) % 6;
    delay(2000);
}
