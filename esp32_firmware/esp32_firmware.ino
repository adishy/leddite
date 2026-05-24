// Leddite V2 — Full Network Firmware
// WebSocket server on port 81. Receives binary sprite packets,
// renders via Canvas + MarqueeEngine, drives 16×16 WS2812B panel.
//
// Run test suite against it:
//   python test_suite.py <esp32-ip> 81
//
// Protocol: 8-byte header + RGB payload (see docs/Components/Protocol.md)

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <FastLED.h>
#include "wifi_credentials.h"
#include "Canvas.h"
#include "ProtocolHandler.h"
#include "MarqueeEngine.h"

// ── Config ────────────────────────────────────────────────────────────
#define LED_PIN              4
#define NUM_LEDS             256
#define BRIGHTNESS           64      // 25%
#define WS_PORT              81
#define MARQUEE_BUFFER_SIZE  4096
#define MARQUEE_SPEED_PPS    20      // pixels per second

// ── State ─────────────────────────────────────────────────────────────
WebSocketsServer webSocket(WS_PORT);
Canvas           canvas;
CRGB             leds[NUM_LEDS];
MarqueeEngine    marquee;
uint8_t          marqueeBuffer[MARQUEE_BUFFER_SIZE];
SpriteHeader     marqueeHeader;

// ── Physical mapping (confirmed via hardware diagnostic) ──────────────
// Column-serpentine: even columns top→bottom, odd columns bottom→top,
// columns ordered right→left (x=15 = physical LED 0).
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

// ── WebSocket event handler ───────────────────────────────────────────
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected\n", num);
            break;

        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
            break;
        }

        case WStype_BIN: {
            SpriteHeader header;
            if (!ProtocolHandler::parseHeader(payload, length, header)) {
                Serial.printf("[%u] Bad packet (len=%u)\n", num, length);
                break;
            }

            const uint8_t* pixelData = payload + ProtocolHandler::HEADER_SIZE;
            size_t         pixelSize = length  - ProtocolHandler::HEADER_SIZE;

            if (header.marqueeActive()) {
                // Start autonomous scrolling
                if (pixelSize <= MARQUEE_BUFFER_SIZE) {
                    memcpy(marqueeBuffer, pixelData, pixelSize);
                    marqueeHeader = header;
                    marquee.start(marqueeBuffer, header.width, header.height,
                                  MARQUEE_SPEED_PPS, millis());
                    Serial.printf("[%u] Marquee start: %ux%u\n", num, header.width, header.height);
                } else {
                    Serial.printf("[%u] Marquee payload too large (%u > %u)\n",
                                  num, pixelSize, MARQUEE_BUFFER_SIZE);
                }
            } else {
                // Static sprite
                marquee.stop();
                FastLED.setBrightness(header.brightness ? header.brightness : BRIGHTNESS);
                canvas.drawSprite(pixelData,
                                  header.width, header.height,
                                  header.x_offset, header.y_offset,
                                  header.rotation,
                                  header.clearCanvas());
                if (header.showImmediately()) {
                    updateDisplay();
                }
            }
            break;
        }

        default:
            break;
    }
}

// ── Setup ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== LEDDITE V2 Network Firmware ===");

    // LEDs — init early so we can show a "connecting" indicator
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

    // Flash green once to signal WiFi connected
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(500);
    FastLED.clear();
    FastLED.show();

    // WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("WebSocket server on ws://%s:%u\n",
                  WiFi.localIP().toString().c_str(), WS_PORT);
    Serial.printf("Run: python test_suite.py %s %u\n",
                  WiFi.localIP().toString().c_str(), WS_PORT);
}

// ── Loop ──────────────────────────────────────────────────────────────
void loop() {
    webSocket.loop();

    // Drive marquee at ~30 FPS when active
    if (marquee.isActive()) {
        static uint32_t lastMarqueeMs = 0;
        uint32_t now = millis();
        if (now - lastMarqueeMs >= 33) {
            lastMarqueeMs = now;
            int16_t xOff = marquee.getXOffset(now);
            canvas.drawSprite(marqueeBuffer,
                              marqueeHeader.width, marqueeHeader.height,
                              xOff, marqueeHeader.y_offset,
                              marqueeHeader.rotation,
                              /*clearBefore=*/true);
            updateDisplay();
        }
    }
}
