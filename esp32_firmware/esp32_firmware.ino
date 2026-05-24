// MAPPING DIAGNOSTIC — shows one pattern at a time, 3s each.
// Watch the display and match what you see to the serial label.
// This will tell us exactly what the correct getPhysicalIndex should be.

#include <FastLED.h>
#include "Canvas.h"

#define LED_PIN    4
#define NUM_LEDS   256
#define BRIGHTNESS 64

CRGB leds[NUM_LEDS];
Canvas canvas;

// Current best-guess mapping (column-based serpentine, derived from legacy firmware).
// We will verify this is correct via the patterns below.
uint16_t getPhysicalIndex(uint8_t x, uint8_t y) {
    if (x % 2 == 0) {
        return (15 - x) * 16 + y;
    } else {
        return (15 - x) * 16 + (15 - y);
    }
}

void updateDisplay() {
    const LedditeCRGB* buf = canvas.getBuffer();
    for (uint8_t y = 0; y < 16; y++) {
        for (uint8_t x = 0; x < 16; x++) {
            LedditeCRGB px = buf[y * 16 + x];
            leds[getPhysicalIndex(x, y)] = CRGB(px.r, px.g, px.b);
        }
    }
    FastLED.show();
}

void showPixel(uint8_t x, uint8_t y, CRGB color) {
    canvas.clear();
    uint8_t px[3] = { color.r, color.g, color.b };
    canvas.drawSprite(px, 1, 1, x, y, 0, false);
    updateDisplay();
}

void showRow(uint8_t row, CRGB color) {
    canvas.clear();
    uint8_t rowbuf[16 * 3];
    for (int i = 0; i < 16; i++) {
        rowbuf[i*3]   = color.r;
        rowbuf[i*3+1] = color.g;
        rowbuf[i*3+2] = color.b;
    }
    canvas.drawSprite(rowbuf, 16, 1, 0, row, 0, false);
    updateDisplay();
}

void showCol(uint8_t col, CRGB color) {
    canvas.clear();
    uint8_t colbuf[16 * 3];
    for (int i = 0; i < 16; i++) {
        colbuf[i*3]   = color.r;
        colbuf[i*3+1] = color.g;
        colbuf[i*3+2] = color.b;
    }
    canvas.drawSprite(colbuf, 1, 16, col, 0, 0, false);
    updateDisplay();
}

void solidFill(CRGB color) {
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
}

void blank() {
    FastLED.clear(); FastLED.show();
}

void step(const char* label, uint32_t ms) {
    Serial.printf("\n>>> %s\n", label);
    delay(ms);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    blank();

    Serial.println("\n=== MAPPING DIAGNOSTIC ===");
    Serial.println("Each step holds for 3s. Note what you see.");

    // ── Sanity: raw fill_solid (no mapping). Should be FULL solid white.
    solidFill(CRGB::White);
    step("SANITY: fill_solid WHITE — entire display should be solid white", 3000);

    blank(); delay(500);

    // ── Canvas solid fills — should look identical to fill_solid above
    uint8_t all[256*3];
    for(int i=0;i<256;i++){all[i*3]=255;all[i*3+1]=255;all[i*3+2]=255;}
    canvas.drawSprite(all, 16, 16, 0, 0, 0, true);
    updateDisplay();
    step("CANVAS solid WHITE — should also be entire display solid white", 3000);

    blank(); delay(500);

    // ── Single pixel: logical top-left corner (0,0) → GREEN dot
    showPixel(0, 0, CRGB::Green);
    step("PIXEL (0,0) GREEN — should be ONE green dot at TOP-LEFT corner", 3000);

    blank(); delay(500);

    // ── Single pixel: logical top-right corner (15,0) → RED dot
    showPixel(15, 0, CRGB::Red);
    step("PIXEL (15,0) RED — should be ONE red dot at TOP-RIGHT corner", 3000);

    blank(); delay(500);

    // ── Single pixel: logical bottom-left corner (0,15) → BLUE dot
    showPixel(0, 15, CRGB::Blue);
    step("PIXEL (0,15) BLUE — should be ONE blue dot at BOTTOM-LEFT corner", 3000);

    blank(); delay(500);

    // ── Single pixel: logical bottom-right (15,15) → YELLOW dot
    showPixel(15, 15, CRGB(255,255,0));
    step("PIXEL (15,15) YELLOW — should be ONE yellow dot at BOTTOM-RIGHT corner", 3000);

    blank(); delay(500);

    // ── Top row (y=0): should be a WHITE horizontal bar across the very top
    showRow(0, CRGB::White);
    step("ROW y=0 WHITE — should be a WHITE bar across the TOP edge", 3000);

    blank(); delay(500);

    // ── Bottom row (y=15): should be a WHITE horizontal bar across the very bottom
    showRow(15, CRGB::White);
    step("ROW y=15 WHITE — should be a WHITE bar across the BOTTOM edge", 3000);

    blank(); delay(500);

    // ── Left column (x=0): should be a WHITE vertical bar on the left edge
    showCol(0, CRGB::White);
    step("COL x=0 WHITE — should be a WHITE bar down the LEFT edge", 3000);

    blank(); delay(500);

    // ── Right column (x=15): should be a WHITE vertical bar on the right edge
    showCol(15, CRGB::White);
    step("COL x=15 WHITE — should be a WHITE bar down the RIGHT edge", 3000);

    blank(); delay(500);

    // ── Raw LED 0 direct (no mapping at all): tells us where the chain STARTS
    leds[0] = CRGB::Green;
    FastLED.show();
    step("RAW leds[0] GREEN — physical LED 0; note which corner it lights", 3000);

    blank(); delay(500);

    // ── Raw LEDs 0-15: shows the first serpentine band direction
    for(int i=0;i<16;i++) leds[i]=CRGB(255,50,0); // orange
    FastLED.show();
    step("RAW leds[0..15] ORANGE — first 16 LEDs; row or column? which end?", 3000);

    blank(); delay(500);

    Serial.println("\n=== DIAGNOSTIC COMPLETE — entering loop (row scan) ===");
}

void loop() {
    // Slowly scan rows top→bottom so you can see the direction
    for (uint8_t r = 0; r < 16; r++) {
        showRow(r, CRGB::White);
        Serial.printf("Row %u\n", r);
        delay(400);
    }
    blank();
    delay(500);
}
