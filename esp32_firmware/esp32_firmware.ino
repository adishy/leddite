// Leddite V2 — Standalone Scene Test Suite
// Mirrors test_suite.py scenes exactly, no WiFi/WebSocket needed.
// Scenes: bouncing ball → shapes → text (static + rotated) → marquee → repeat.

#include <FastLED.h>
#include <math.h>
#include "Canvas.h"
#include "MarqueeEngine.h"

#define LED_PIN    4
#define NUM_LEDS   256
#define BRIGHTNESS 64

CRGB          leds[NUM_LEDS];
Canvas        canvas;
MarqueeEngine marquee;

// ── Physical mapping (confirmed) ─────────────────────────────────────
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

void blank() { FastLED.clear(); FastLED.show(); }

// ── 5×7 Bitmap Font ──────────────────────────────────────────────────
// Each char = 5 column bytes; bit0=top row, bit6=bottom row.
// Exactly matches the FONT dict in test_suite.py.
struct GlyphEntry { char ch; uint8_t cols[5]; };
static const GlyphEntry FONT[] = {
    {'0',{0x3E,0x51,0x49,0x51,0x3E}}, {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'2',{0x42,0x61,0x51,0x49,0x46}}, {'3',{0x22,0x41,0x49,0x49,0x36}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}}, {'5',{0x2F,0x49,0x49,0x49,0x31}},
    {'6',{0x3E,0x4A,0x49,0x49,0x30}}, {'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'A',{0x7E,0x11,0x11,0x11,0x7E}}, {'B',{0x7F,0x49,0x49,0x49,0x36}},
    {'C',{0x3E,0x41,0x41,0x41,0x22}}, {'D',{0x7F,0x41,0x41,0x22,0x1C}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}}, {'F',{0x7F,0x09,0x09,0x09,0x01}},
    {'G',{0x3E,0x41,0x49,0x49,0x7A}}, {'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'I',{0x00,0x41,0x7F,0x41,0x00}}, {'J',{0x20,0x40,0x41,0x3F,0x01}},
    {'K',{0x7F,0x08,0x14,0x22,0x41}}, {'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'M',{0x7F,0x02,0x0C,0x02,0x7F}}, {'N',{0x7F,0x04,0x08,0x10,0x7F}},
    {'O',{0x3E,0x41,0x41,0x41,0x3E}}, {'P',{0x7F,0x09,0x09,0x09,0x06}},
    {'Q',{0x3E,0x41,0x51,0x21,0x5E}}, {'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}}, {'T',{0x01,0x01,0x7F,0x01,0x01}},
    {'U',{0x3F,0x40,0x40,0x40,0x3F}}, {'V',{0x1F,0x20,0x40,0x20,0x1F}},
    {'W',{0x3F,0x40,0x38,0x40,0x3F}}, {'X',{0x63,0x14,0x08,0x14,0x63}},
    {'Y',{0x07,0x08,0x70,0x08,0x07}}, {'Z',{0x61,0x51,0x49,0x45,0x43}},
    {' ',{0x00,0x00,0x00,0x00,0x00}}, {'!',{0x00,0x00,0x5F,0x00,0x00}},
    {'?',{0x02,0x01,0x51,0x09,0x06}},
};

static const uint8_t SPACE_GLYPH[5] = {0,0,0,0,0};

const uint8_t* getGlyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32; // uppercase
    for (auto& e : FONT) if (e.ch == c) return e.cols;
    return SPACE_GLYPH;
}

// Render text into a heap buffer; caller must free[].
// width = len * 6, height = 7. Returns pixel buffer (RGB, row-major).
uint8_t* renderText(const char* text, uint8_t r, uint8_t g, uint8_t b,
                    uint16_t& outW, uint16_t& outH) {
    uint16_t len = strlen(text);
    outW = len * 6;
    outH = 7;
    uint8_t* buf = new uint8_t[outW * outH * 3]();  // zero-init
    for (uint16_t ci = 0; ci < len; ci++) {
        const uint8_t* cols = getGlyph(text[ci]);
        for (uint8_t col = 0; col < 5; col++) {
            for (uint8_t row = 0; row < 7; row++) {
                if ((cols[col] >> row) & 1) {
                    uint16_t px = ci * 6 + col;
                    uint16_t py = row;
                    uint32_t idx = (py * outW + px) * 3;
                    buf[idx]   = r;
                    buf[idx+1] = g;
                    buf[idx+2] = b;
                }
            }
        }
    }
    return buf;
}

// ── Scene 1: Bouncing Ball ───────────────────────────────────────────
// 2×2 yellow ball bouncing around, 60 FPS, 120 frames (~2s).
// Mirrors test_bouncing_ball() in test_suite.py exactly.
void scene_bouncing_ball() {
    Serial.println("Scene: Bouncing Ball");
    float x=8, y=8, dx=0.8f, dy=0.6f;
    for (int frame = 0; frame < 200; frame++) {
        x += dx; y += dy;
        if (x <= 0 || x >= 14) dx = -dx;
        if (y <= 0 || y >= 14) dy = -dy;
        uint8_t ball[4*3] = {255,255,0, 255,255,0, 255,255,0, 255,255,0};
        canvas.drawSprite(ball, 2, 2, (int8_t)x, (int8_t)y, 0, /*clearBefore=*/true);
        updateDisplay();
        delay(16); // ~60 FPS
    }
    blank(); delay(300);
}

// ── Scene 2: Shapes ──────────────────────────────────────────────────
// Red 4×4 block at (2,2), then blue approximate-circle at (9,9) layered on top.
// Mirrors test_shapes() in test_suite.py.
void scene_shapes() {
    Serial.println("Scene: Shapes");

    // Red 4×4 block at (2,2), clear canvas first
    uint8_t red[4*4*3];
    for (int i = 0; i < 16; i++) { red[i*3]=255; red[i*3+1]=0; red[i*3+2]=0; }
    canvas.drawSprite(red, 4, 4, 2, 2, 0, /*clearBefore=*/true);
    updateDisplay();
    delay(1000);

    // Blue circle-ish (5×5, pixels within radius 2.5) at (9,9), overlay
    uint8_t circle[5*5*3] = {};
    for (int sy = 0; sy < 5; sy++)
        for (int sx = 0; sx < 5; sx++) {
            float dist = sqrtf((sx-2)*(sx-2) + (sy-2)*(sy-2));
            if (dist < 2.5f) {
                int i = (sy*5+sx)*3;
                circle[i]=0; circle[i+1]=0; circle[i+2]=255;
            }
        }
    canvas.drawSprite(circle, 5, 5, 9, 9, 0, /*clearBefore=*/false);
    updateDisplay();
    delay(1500);

    blank(); delay(300);
}

// ── Scene 3: Text ────────────────────────────────────────────────────
// "HI" in red at (2,4), then "LED" in blue rotated 90° at (8,0).
// Mirrors test_text() in test_suite.py.
void scene_text() {
    Serial.println("Scene: Text");
    uint16_t w, h;

    // "HI" red, no rotation
    uint8_t* hi = renderText("HI", 255, 0, 0, w, h);
    canvas.drawSprite(hi, w, h, 2, 4, 0, /*clearBefore=*/true);
    delete[] hi;
    updateDisplay();
    delay(1500);

    // "LED" blue, 90° CW rotation, at (8,0)
    uint8_t* led = renderText("LED", 0, 100, 255, w, h);
    canvas.drawSprite(led, w, h, 8, 0, 1, /*clearBefore=*/true);
    delete[] led;
    updateDisplay();
    delay(1500);

    blank(); delay(300);
}

// ── Scene 4: Marquee ─────────────────────────────────────────────────
// Scrolls "LEDDITE V2 !" in orange for ~8 seconds using MarqueeEngine.
// Mirrors test_marquee() in test_suite.py.

#define MARQUEE_BUF_SIZE 4096
static uint8_t marqueeBuf[MARQUEE_BUF_SIZE];

void scene_marquee() {
    Serial.println("Scene: Marquee");
    uint16_t w, h;
    uint8_t* txt = renderText("LEDDITE V2 !", 255, 128, 0, w, h);
    uint32_t pixBytes = w * h * 3;
    if (pixBytes <= MARQUEE_BUF_SIZE) {
        memcpy(marqueeBuf, txt, pixBytes);
        marquee.start(marqueeBuf, w, h, 20, millis());
    }
    delete[] txt;

    uint32_t end = millis() + 8000;
    while (millis() < end) {
        int16_t xOff = marquee.getXOffset(millis());
        canvas.drawSprite(marqueeBuf, w, h, xOff, 4, 0, /*clearBefore=*/true);
        updateDisplay();
        delay(33); // ~30 FPS
    }
    marquee.stop();
    blank(); delay(300);
}

// ── Setup & Loop ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== LEDDITE V2 SCENE TEST SUITE (standalone) ===");

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    blank();
    delay(500);
}

void loop() {
    scene_bouncing_ball();
    scene_shapes();
    scene_text();
    scene_marquee();
}
