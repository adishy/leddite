#include "MarqueeEngine.h"

MarqueeEngine::MarqueeEngine() : active(false), startTime(0), width(0), speed_pps(0) {}

void MarqueeEngine::start(const uint8_t* data, uint16_t w, uint16_t h, uint16_t speed, uint32_t currentTimeMs) {
    active = true;
    width = w;
    speed_pps = speed;
    startTime = currentTimeMs;
}

void MarqueeEngine::stop() {
    active = false;
}

int16_t MarqueeEngine::getXOffset(uint32_t currentTimeMs) const {
    if (!active || width <= 16) return 0;
    
    // Total distance to scroll: width + 16 (to fully scroll off)
    // Actually, let's just scroll from 0 to -(width - 16) or loop.
    // If it's a marquee, it usually scrolls from right to left.
    // Let's start with x=16 and scroll to x=-width.
    
    uint32_t elapsedMs = currentTimeMs - startTime;
    uint32_t totalDurationMs = ((width + 16) * 1000) / speed_pps;
    
    uint32_t currentProgressMs = elapsedMs % totalDurationMs;
    int16_t currentX = 16 - (currentProgressMs * (width + 16)) / totalDurationMs;
    
    return currentX;
}
