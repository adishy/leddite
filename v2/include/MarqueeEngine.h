#ifndef MARQUEE_ENGINE_H
#define MARQUEE_ENGINE_H

#include <stdint.h>
#include <stddef.h>

class MarqueeEngine {
public:
    MarqueeEngine();
    
    // Configures the marquee with sprite data and speed.
    // Speed is in pixels per second.
    void start(const uint8_t* data, uint16_t w, uint16_t h, uint16_t speed, uint32_t currentTimeMs);
    void stop();
    
    // Calculates the current X offset based on elapsed time.
    int16_t getXOffset(uint32_t currentTimeMs) const;
    
    bool isActive() const { return active; }

private:
    bool active;
    uint32_t startTime;
    uint16_t width;
    uint16_t speed_pps; // Pixels per second
};

#endif
