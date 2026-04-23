#include <WiFi.h>
#include <WebSocketsServer.h>
#include <FastLED.h>
#include "Canvas.h"
#include "ProtocolHandler.h"
#include "MarqueeEngine.h"

// --- Configuration ---
#define LED_PIN         4
#define NUM_LEDS        256
#define BRIGHTNESS      64
#define WS_PORT         81
#define MARQUEE_BUFFER_SIZE 4096

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// --- State ---
WebSocketsServer webSocket = WebSocketsServer(WS_PORT);
Canvas canvas;
CRGB leds[NUM_LEDS];
MarqueeEngine marquee;
uint8_t marqueeBuffer[MARQUEE_BUFFER_SIZE];
SpriteHeader marqueeHeader;

// --- Hardware Mapping ---
uint16_t getPhysicalIndex(uint8_t x, uint8_t y) {
    uint16_t i;
    if (y % 2 == 0) {
        i = (y * 16) + x;
    } else {
        i = (y * 16) + (15 - x);
    }
    return i;
}

void updateDisplay() {
    const LedditeCRGB* buffer = (const LedditeCRGB*)canvas.getBuffer();
    for (uint8_t y = 0; y < 16; y++) {
        for (uint8_t x = 0; x < 16; x++) {
            LedditeCRGB pixel = buffer[y * 16 + x];
            leds[getPhysicalIndex(x, y)] = CRGB(pixel.r, pixel.g, pixel.b);
        }
    }
    FastLED.show();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, length_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
            break;
        }
        case WStype_BIN: {
            SpriteHeader header;
            if (ProtocolHandler::parseHeader(payload, length, header)) {
                const uint8_t* pixelData = payload + ProtocolHandler::HEADER_SIZE;
                size_t pixelSize = length - ProtocolHandler::HEADER_SIZE;
                
                if (header.marqueeActive()) {
                    // Start Marquee
                    if (pixelSize <= MARQUEE_BUFFER_SIZE) {
                        memcpy(marqueeBuffer, pixelData, pixelSize);
                        marqueeHeader = header;
                        // Speed logic: for now fixed at 20 pps or use brightness field for speed?
                        // Let's use version field or just 20.
                        marquee.start(marqueeBuffer, header.width, header.height, 20, millis());
                    }
                } else {
                    // Static Sprite
                    marquee.stop();
                    canvas.drawSprite(pixelData, header.width, header.height, 
                                     header.x_offset, header.y_offset, 
                                     header.rotation, header.clearCanvas());
                    
                    FastLED.setBrightness(header.brightness);
                    if (header.showImmediately()) {
                        updateDisplay();
                    }
                }
            }
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);

    // WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // LEDs
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // WebSocket
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Serial.printf("Leddite V2 Server started on port %u\n", WS_PORT);
}

void loop() {
    webSocket.loop();
    
    if (marquee.isActive()) {
        static uint32_t lastUpdate = 0;
        if (millis() - lastUpdate > 33) { // 30 FPS
            lastUpdate = millis();
            int16_t xOff = marquee.getXOffset(millis());
            canvas.drawSprite(marqueeBuffer, marqueeHeader.width, marqueeHeader.height, 
                             xOff, marqueeHeader.y_offset, 
                             marqueeHeader.rotation, true);
            updateDisplay();
        }
    }
}
