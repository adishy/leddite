// Leddite V2 — Boot Menu + Multi-Mode Firmware
//
// Modes (selected via rotary encoder boot menu):
//   CK  Clock + Calendar  — NTP time (ET) alternating with date marquee
//   NT  Network Canvas    — existing WebSocket binary protocol (unchanged)
//   PT  Pattern Slideshow — rainbow, lava lamp, pulse, sparkle
//   TM  Visual Timer      — rotary encoder sets minutes, progress bar countdown
//
// Encoder: CLK=32, DT=33, SW=25
//   Menu:    turn = navigate, press = select
//   CK/PT:   press = back to menu
//   TM:      turn = adjust minutes / press = start/cancel / press when done = menu
//   NT:      encoder events broadcast as JSON to connected clients
//            long-press (2s) = back to menu
//
// Binary WebSocket protocol on port 81: UNCHANGED from V2 original.
// Encoder state broadcast (Network mode only):
//   {"type":"encoder","delta":1}         — clockwise turn
//   {"type":"encoder","delta":-1}        — counter-clockwise turn
//   {"type":"encoder","button":"pressed"} — short press
//
// Run test suite against Network mode:
//   python test_suite.py <esp32-ip> 81

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <FastLED.h>
#include <time.h>

#include "wifi_credentials.h"
#include "Canvas.h"
#include "ProtocolHandler.h"
#include "MarqueeEngine.h"

#include "AppState.h"
#include "EncoderInput.h"
#include "MenuMode.h"
#include "TimeMode.h"
#include "PatternMode.h"
#include "TimerMode.h"
#include "NetworkMode.h"

// ── Config ────────────────────────────────────────────────────────────────────
#define LED_PIN              4
#define NUM_LEDS             256
#define BRIGHTNESS           64      // default 25%
#define WS_PORT              81
#define MARQUEE_BUFFER_SIZE  4096

// ── Global state ──────────────────────────────────────────────────────────────
WebSocketsServer webSocket(WS_PORT);
Canvas           canvas;
CRGB             leds[NUM_LEDS];
MarqueeEngine    marquee;
uint8_t          marqueeBuffer[MARQUEE_BUFFER_SIZE];

EncoderInput encoder;
MenuMode     menuMode;
TimeMode     timeMode;
PatternMode  patternMode;
TimerMode    timerMode;
NetworkMode  networkMode;

AppMode currentMode = AppMode::MENU;

// ── Physical LED mapping (column-serpentine, confirmed via hardware diagnostic)
// Even columns top→bottom, odd columns bottom→top, columns ordered right→left.
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

// ── WebSocket event callback (global — delegates to NetworkMode) ──────────────
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (currentMode == AppMode::NETWORK) {
        networkMode.onWebSocketEvent(canvas, marquee, webSocket,
                                     num, type, payload, length);
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== LEDDITE V2 Multi-Mode Firmware ===");

    // LEDs — init early for visual feedback during boot
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // WiFi
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // NTP — Eastern Time with automatic DST (EST/EDT)
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();
    Serial.print("NTP sync");
    for (int i = 0; i < 10 && time(nullptr) < 1000000000UL; i++) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(time(nullptr) >= 1000000000UL ? " OK" : " timeout (will retry)");

    // Green flash: WiFi + NTP ready
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(500);
    FastLED.clear();
    FastLED.show();

    // Encoder
    encoder.begin();
    Serial.println("Encoder ready (CLK=32, DT=33, SW=25)");

    // WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("WebSocket on ws://%s:%u\n",
                  WiFi.localIP().toString().c_str(), WS_PORT);

    // Mode objects
    networkMode.begin(canvas, webSocket, marquee, marqueeBuffer, MARQUEE_BUFFER_SIZE);

    // Start in menu
    currentMode = AppMode::MENU;
    menuMode.begin(canvas);
    updateDisplay();

    Serial.println("Boot menu: rotate encoder to navigate, press to select");
}

// ── Shared helper: go back to boot menu ──────────────────────────────────────
static void goToMenu() {
    marquee.stop();
    currentMode = AppMode::MENU;
    FastLED.setBrightness(BRIGHTNESS);  // reset brightness in case Network changed it
    menuMode.begin(canvas);
    Serial.println("[→ Menu]");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    EncoderEvent ev = encoder.poll();

    // ── Universal long-press: back to menu from anywhere (3s hold) ────────────
    // Checked before per-mode dispatch so every mode gets "home" for free.
    // Exception: in MENU itself a long press is a no-op (already home).
    if (ev.longPress && currentMode != AppMode::MENU) {
        goToMenu();
        updateDisplay();
        return;
    }

    switch (currentMode) {

        // ── MENU ──────────────────────────────────────────────────────────────
        case AppMode::MENU:
            if (ev.delta) {
                menuMode.onEncoderTurn(ev.delta, canvas);
            }
            if (ev.pressed) {
                currentMode = menuMode.onEncoderPress(canvas);
                switch (currentMode) {
                    case AppMode::CLOCK_CAL:
                        Serial.println("[Menu] → Clock + Calendar");
                        timeMode.begin(canvas, marquee);
                        break;
                    case AppMode::NETWORK:
                        Serial.printf("[Menu] → Network Canvas  ws://%s:%u\n",
                                      WiFi.localIP().toString().c_str(), WS_PORT);
                        FastLED.setBrightness(BRIGHTNESS);
                        canvas.clear();
                        break;
                    case AppMode::PATTERN:
                        Serial.println("[Menu] → Pattern Slideshow");
                        patternMode.begin(canvas);
                        break;
                    case AppMode::TIMER:
                        Serial.println("[Menu] → Visual Timer");
                        timerMode.begin(canvas);
                        break;
                    default:
                        break;
                }
            }
            break;

        // ── CLOCK + CALENDAR ──────────────────────────────────────────────────
        case AppMode::CLOCK_CAL:
            // Short press: toggle clock↔date display early (don't wait 5s)
            if (ev.pressed) {
                timeMode.toggleDisplay(canvas, marquee);
            }
            timeMode.update(canvas, marquee);
            break;

        // ── PATTERN SLIDESHOW ─────────────────────────────────────────────────
        case AppMode::PATTERN:
            // Short press OR turn: advance to next pattern
            if (ev.pressed || ev.delta) {
                patternMode.nextPattern();
                Serial.println("[Pattern] Next");
            }
            patternMode.update(canvas);
            break;

        // ── VISUAL TIMER ──────────────────────────────────────────────────────
        case AppMode::TIMER:
            if (ev.delta) {
                timerMode.onEncoderTurn(ev.delta);
            }
            if (ev.pressed) {
                if (timerMode.onEncoderPress(canvas)) {
                    goToMenu();
                    break;
                }
            }
            timerMode.update(canvas);
            break;

        // ── NETWORK CANVAS ────────────────────────────────────────────────────
        case AppMode::NETWORK:
            // Short press / turn: broadcast encoder event to WS clients
            // Long press is handled above (universal home gesture)
            if (ev.delta || ev.pressed || ev.released) {
                networkMode.handleEncoder(ev, webSocket);
            }
            networkMode.update(canvas, webSocket, marquee);
            break;

        default:
            break;
    }

    updateDisplay();
}
