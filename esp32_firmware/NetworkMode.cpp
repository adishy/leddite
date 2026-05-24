#include "NetworkMode.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// ── Public API ────────────────────────────────────────────────────────────────

void NetworkMode::begin(Canvas& canvas, WebSocketsServer& ws,
                        MarqueeEngine& marquee, uint8_t* buf, size_t bufSize) {
    (void)canvas; (void)ws; (void)marquee;  // WebSocket was started in setup()
    marqueeBuf      = buf;
    marqueeBufSize  = bufSize;
    lastMarqueeMs   = 0;
    lastBrightness  = DEFAULT_BRIGHTNESS;
    Serial.println("[Network] Mode ready — WebSocket on port 81");
    Serial.println("[Network] Rotate encoder → JSON events broadcast to clients");
    Serial.println("[Network] Long-press encoder → return to menu");
}

void NetworkMode::update(Canvas& canvas, WebSocketsServer& ws, MarqueeEngine& marquee) {
    ws.loop();

    // Drive marquee scroll at ~30 FPS when active
    if (marquee.isActive()) {
        uint32_t now = millis();
        if (now - lastMarqueeMs >= 33) {
            lastMarqueeMs = now;
            int16_t xOff = marquee.getXOffset(now);
            canvas.drawSprite(marqueeBuf,
                              lastMarqueeHeader.width,
                              lastMarqueeHeader.height,
                              (int8_t)xOff,
                              lastMarqueeHeader.y_offset,
                              lastMarqueeHeader.rotation,
                              /*clearBefore=*/true);
        }
    }
}

void NetworkMode::handleEncoder(const EncoderEvent& ev, WebSocketsServer& ws) {
    char buf[80];

    if (ev.delta != 0) {
        snprintf(buf, sizeof(buf), "{\"type\":\"encoder\",\"delta\":%d}", ev.delta);
        ws.broadcastTXT(buf);
        Serial.printf("[Network] Encoder broadcast: delta=%d\n", ev.delta);
    }
    if (ev.pressed) {
        ws.broadcastTXT("{\"type\":\"encoder\",\"button\":\"pressed\"}");
        Serial.println("[Network] Encoder broadcast: pressed");
    }
    if (ev.released) {
        ws.broadcastTXT("{\"type\":\"encoder\",\"button\":\"released\"}");
        Serial.println("[Network] Encoder broadcast: released");
    }
}

void NetworkMode::onWebSocketEvent(Canvas& canvas, MarqueeEngine& marquee,
                                   WebSocketsServer& ws,
                                   uint8_t num, WStype_t type,
                                   uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected\n", num);
            break;

        case WStype_CONNECTED: {
            IPAddress ip = ws.remoteIP(num);
            Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
            break;
        }

        case WStype_BIN: {
            SpriteHeader header;
            if (!ProtocolHandler::parseHeader(payload, length, header)) {
                Serial.printf("[%u] Bad packet (len=%u)\n", num, (unsigned)length);
                break;
            }

            const uint8_t* pixelData = payload + ProtocolHandler::HEADER_SIZE;
            size_t         pixelSize = length  - ProtocolHandler::HEADER_SIZE;

            if (header.marqueeActive()) {
                if (pixelSize <= marqueeBufSize) {
                    memcpy(marqueeBuf, pixelData, pixelSize);
                    lastMarqueeHeader = header;
                    marquee.start(marqueeBuf, header.width, header.height,
                                  MARQUEE_SPEED_PPS, millis());
                    Serial.printf("[%u] Marquee start: %ux%u\n",
                                  num, header.width, header.height);
                } else {
                    Serial.printf("[%u] Marquee payload too large (%u > %u)\n",
                                  num, (unsigned)pixelSize, (unsigned)marqueeBufSize);
                }
            } else {
                // Static sprite
                marquee.stop();
                lastBrightness = header.brightness ? header.brightness : DEFAULT_BRIGHTNESS;
                FastLED.setBrightness(lastBrightness);

                canvas.drawSprite(pixelData,
                                  header.width, header.height,
                                  header.x_offset, header.y_offset,
                                  header.rotation,
                                  header.clearCanvas());
                // showImmediately: main loop calls updateDisplay() each iteration
                // so max latency is one loop tick (~1ms at idle, ~33ms under marquee load)
            }
            break;
        }

        default:
            break;
    }
}
