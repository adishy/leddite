#pragma once

#include "Canvas.h"
#include "MarqueeEngine.h"
#include "ProtocolHandler.h"
#include "EncoderInput.h"
#include <WebSocketsServer.h>
#include <FastLED.h>
#include <stdint.h>

// NetworkMode — wraps the existing WebSocket binary protocol server.
//
// Identical behavior to the original single-mode firmware, with one addition:
// encoder events are broadcast as JSON TEXT frames to all connected clients.
//
// Design note: showImmediately packets are rendered to Canvas immediately and
// displayed on the next main loop updateDisplay() call (< 33ms latency).
// Marquee sprites are scrolled at ~30 FPS inside update().
//
// Exit: long press on encoder (handled by main .ino loop, not this class).
class NetworkMode {
public:
    static const uint16_t MARQUEE_SPEED_PPS  = 20;
    static const uint8_t  DEFAULT_BRIGHTNESS = 64;

    void begin(Canvas& canvas, WebSocketsServer& ws,
               MarqueeEngine& marquee, uint8_t* marqueeBuf, size_t marqueeBufSize);

    // Call each loop() — processes WebSocket messages, drives marquee scroll.
    // brightness is the current FastLED brightness level (may be updated by packets).
    void update(Canvas& canvas, WebSocketsServer& ws, MarqueeEngine& marquee);

    // Broadcast encoder event as JSON TEXT to all connected clients
    void handleEncoder(const EncoderEvent& ev, WebSocketsServer& ws);

    // WebSocket event handler — called from global webSocketEvent() in .ino
    void onWebSocketEvent(Canvas& canvas, MarqueeEngine& marquee,
                          WebSocketsServer& ws,
                          uint8_t num, WStype_t type,
                          uint8_t* payload, size_t length);

    // Last brightness from a packet (0 = use default) — read by main .ino
    uint8_t lastBrightness = DEFAULT_BRIGHTNESS;

    // Canvas readback response magic byte
    static const uint8_t CANVAS_ACK_MAGIC = 0xCA;

private:
    // Send current canvas buffer back to client num as a binary WS frame.
    // Format: [0xCA, width=16, height=16, r,g,b × 256] = 771 bytes.
    void sendCanvasAck(const Canvas& canvas, WebSocketsServer& ws, uint8_t num);
    uint8_t* marqueeBuf     = nullptr;
    size_t   marqueeBufSize = 0;
    uint32_t lastMarqueeMs  = 0;
    SpriteHeader lastMarqueeHeader;
};
