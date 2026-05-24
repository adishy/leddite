// Leddite V2 — Standalone Visual Test Suite (no network)
// Runs a full sequence of Canvas-based tests, then loops solid colors.
// All 256 LEDs via confirmed column-serpentine physical mapping.

#include <FastLED.h>
#include "Canvas.h"
#include "Transformer.h"

#define LED_PIN    4
#define NUM_LEDS   256
#define BRIGHTNESS 64   // 25%

CRGB     leds[NUM_LEDS];
Canvas   canvas;

// ── Physical mapping (confirmed via mapping diagnostic) ──────────────
// Panel is mounted 90° rotated; serpentine runs column-by-column right→left.
// Even columns (x=0,2,…): top→bottom. Odd columns: bottom→top.
uint16_t getPhysicalIndex(uint8_t x, uint8_t y) {
    if (x % 2 == 0) return (15 - x) * 16 + y;
    else             return (15 - x) * 16 + (15 - y);
}

void updateDisplay() {
    const LedditeCRGB* buf = canvas.getBuffer();
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++) {
            LedditeCRGB p = buf[y * 16 + x];
            leds[getPhysicalIndex(x, y)] = CRGB(p.r, p.g, p.b);
        }
    FastLED.show();
}

// ── Canvas helpers ───────────────────────────────────────────────────
void canvasSolidFill(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t px[256 * 3];
    for (int i = 0; i < 256; i++) { px[i*3]=r; px[i*3+1]=g; px[i*3+2]=b; }
    canvas.drawSprite(px, 16, 16, 0, 0, 0, /*clearBefore=*/true);
    updateDisplay();
}

void canvasRow(uint8_t row, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t px[16 * 3];
    for (int i = 0; i < 16; i++) { px[i*3]=r; px[i*3+1]=g; px[i*3+2]=b; }
    canvas.drawSprite(px, 16, 1, 0, row, 0, /*clearBefore=*/true);
    updateDisplay();
}

void canvasCol(uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t px[16 * 3];
    for (int i = 0; i < 16; i++) { px[i*3]=r; px[i*3+1]=g; px[i*3+2]=b; }
    canvas.drawSprite(px, 1, 16, col, 0, 0, /*clearBefore=*/true);
    updateDisplay();
}

void canvasPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t px[3] = {r, g, b};
    canvas.drawSprite(px, 1, 1, x, y, 0, /*clearBefore=*/true);
    updateDisplay();
}

void canvasRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                uint8_t r, uint8_t g, uint8_t b, bool clear = true) {
    uint8_t* px = new uint8_t[w * h * 3];
    for (int i = 0; i < w * h; i++) { px[i*3]=r; px[i*3+1]=g; px[i*3+2]=b; }
    canvas.drawSprite(px, w, h, x, y, 0, clear);
    delete[] px;
    updateDisplay();
}

void blank() { FastLED.clear(); FastLED.show(); }

// ── Test 1: Solid color fills ────────────────────────────────────────
void test_solid_colors() {
    Serial.println("TEST 1: Solid colors");
    const struct { uint8_t r,g,b; const char* name; } colors[] = {
        {255,   0,   0, "RED"},
        {  0, 255,   0, "GREEN"},
        {  0,   0, 255, "BLUE"},
        {255, 255,   0, "YELLOW"},
        {  0, 255, 255, "CYAN"},
        {255,   0, 255, "MAGENTA"},
        {255, 255, 255, "WHITE"},
    };
    for (auto& c : colors) {
        Serial.printf("  %s\n", c.name);
        canvasSolidFill(c.r, c.g, c.b);
        delay(1000);
    }
    blank(); delay(300);
}

// ── Test 2: Row scan ─────────────────────────────────────────────────
void test_row_scan() {
    Serial.println("TEST 2: Row scan (white bar top→bottom)");
    for (uint8_t row = 0; row < 16; row++) {
        canvasRow(row, 255, 255, 255);
        Serial.printf("  Row %u\n", row);
        delay(200);
    }
    blank(); delay(300);
}

// ── Test 3: Column scan ──────────────────────────────────────────────
void test_col_scan() {
    Serial.println("TEST 3: Column scan (white bar left→right)");
    for (uint8_t col = 0; col < 16; col++) {
        canvasCol(col, 255, 255, 255);
        Serial.printf("  Col %u\n", col);
        delay(200);
    }
    blank(); delay(300);
}

// ── Test 4: Corner pixels ────────────────────────────────────────────
void test_corners() {
    Serial.println("TEST 4: Corner pixels");
    Serial.println("  (0,0)=GREEN");   canvasPixel( 0,  0,   0, 255,   0); delay(1000);
    Serial.println("  (15,0)=RED");    canvasPixel(15,  0, 255,   0,   0); delay(1000);
    Serial.println("  (0,15)=BLUE");   canvasPixel( 0, 15,   0,   0, 255); delay(1000);
    Serial.println("  (15,15)=YELLOW");canvasPixel(15, 15, 255, 255,   0); delay(1000);
    blank(); delay(300);
}

// ── Test 5: Pixel walk (4×4 top-left grid) ──────────────────────────
void test_pixel_walk() {
    Serial.println("TEST 5: Pixel walk 4x4 top-left");
    for (uint8_t y = 0; y < 4; y++)
        for (uint8_t x = 0; x < 4; x++) {
            canvasPixel(x, y, 255, 255, 255);
            Serial.printf("  (%u,%u)\n", x, y);
            delay(300);
        }
    blank(); delay(300);
}

// ── Test 6: Rectangles & layering ───────────────────────────────────
void test_rects() {
    Serial.println("TEST 6: Rectangles");

    // Full-screen red background
    Serial.println("  Full red BG");
    canvasSolidFill(180, 0, 0);
    delay(1000);

    // 8x8 green block centred
    Serial.println("  8x8 green block at (4,4)");
    canvasRect(4, 4, 8, 8, 0, 200, 0, /*clear=*/false);
    delay(1000);

    // 4x4 blue block centred on top
    Serial.println("  4x4 blue block at (6,6)");
    canvasRect(6, 6, 4, 4, 0, 0, 255, /*clear=*/false);
    delay(1500);

    blank(); delay(300);
}

// ── Test 7: Rotation ─────────────────────────────────────────────────
// Draw a 4×8 asymmetric sprite (red left half, blue right half) at each rotation.
void test_rotation() {
    Serial.println("TEST 7: Rotation (4x8 sprite, red|blue halves)");
    uint8_t sprite[4 * 8 * 3];
    for (int sy = 0; sy < 8; sy++)
        for (int sx = 0; sx < 4; sx++) {
            int i = (sy * 4 + sx) * 3;
            if (sx < 2) { sprite[i]=200; sprite[i+1]=0;   sprite[i+2]=0;   } // red
            else        { sprite[i]=0;   sprite[i+1]=0;   sprite[i+2]=200; } // blue
        }

    const char* labels[] = {"0°","90°CW","180°","270°CW"};
    for (uint8_t rot = 0; rot < 4; rot++) {
        canvas.clear();
        canvas.drawSprite(sprite, 4, 8, 4, 4, rot, /*clearBefore=*/true);
        updateDisplay();
        Serial.printf("  Rotation %s\n", labels[rot]);
        delay(1500);
    }
    blank(); delay(300);
}

// ── Test 8: Checkerboard ─────────────────────────────────────────────
void test_checkerboard() {
    Serial.println("TEST 8: Checkerboard (alternating pixels)");
    canvas.clear();
    for (uint8_t y = 0; y < 16; y++)
        for (uint8_t x = 0; x < 16; x++) {
            uint8_t px[3];
            if ((x + y) % 2 == 0) { px[0]=255; px[1]=255; px[2]=255; }
            else                   { px[0]=0;   px[1]=0;   px[2]=0;   }
            canvas.drawSprite(px, 1, 1, x, y, 0, false);
        }
    updateDisplay();
    delay(2000);
    blank(); delay(300);
}

// ── Test 9: Rainbow rows ─────────────────────────────────────────────
void test_rainbow() {
    Serial.println("TEST 9: Rainbow rows");
    canvas.clear();
    // Hue across 0-255 over 16 rows
    for (uint8_t row = 0; row < 16; row++) {
        CRGB c = CHSV(row * 16, 255, 220);
        uint8_t px[16 * 3];
        for (int i = 0; i < 16; i++) { px[i*3]=c.r; px[i*3+1]=c.g; px[i*3+2]=c.b; }
        canvas.drawSprite(px, 16, 1, 0, row, 0, false);
    }
    updateDisplay();
    delay(3000);
    blank(); delay(300);
}

// ── Setup ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== LEDDITE V2 VISUAL TEST SUITE (standalone) ===");

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    blank();
    delay(500);

    test_solid_colors();
    test_row_scan();
    test_col_scan();
    test_corners();
    test_pixel_walk();
    test_rects();
    test_rotation();
    test_checkerboard();
    test_rainbow();

    Serial.println("\n=== ALL TESTS DONE — looping solid colors ===");
}

// ── Loop: cycle solid colors forever ────────────────────────────────
void loop() {
    const struct { uint8_t r,g,b; } cols[] = {
        {255,0,0}, {0,255,0}, {0,0,255}, {255,255,0}, {0,255,255}, {255,0,255}
    };
    static uint8_t idx = 0;
    canvasSolidFill(cols[idx].r, cols[idx].g, cols[idx].b);
    idx = (idx + 1) % 6;
    delay(1500);
}
