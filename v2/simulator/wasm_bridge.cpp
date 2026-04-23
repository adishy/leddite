#include "Canvas.h"
#include "Transformer.h"
#include "MarqueeEngine.h"
#include <emscripten/bind.h>

using namespace emscripten;

// Wrapper for the Canvas to expose pixel data
class CanvasWrapper {
public:
    Canvas canvas;
    MarqueeEngine marquee;
    uint8_t marqueeBuffer[4096];
    uint8_t marqueeW, marqueeH, marqueeRot, marqueeY;

    void drawSprite(uintptr_t dataPtr, uint8_t w, uint8_t h, int8_t x, int8_t y, uint8_t rotation, bool clearBefore) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(dataPtr);
        canvas.drawSprite(data, w, h, x, y, rotation, clearBefore);
    }

    void startMarquee(uintptr_t dataPtr, uint8_t w, uint8_t h, uint8_t rotation, int8_t y, uint32_t currentTimeMs) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(dataPtr);
        size_t size = w * h * 3;
        if (size > 4096) size = 4096;
        memcpy(marqueeBuffer, data, size);
        marqueeW = w; marqueeH = h; marqueeRot = rotation; marqueeY = y;
        marquee.start(marqueeBuffer, w, h, 20, currentTimeMs);
    }

    void updateMarquee(uint32_t currentTimeMs) {
        if (marquee.isActive()) {
            int16_t xOff = marquee.getXOffset(currentTimeMs);
            canvas.drawSprite(marqueeBuffer, marqueeW, marqueeH, xOff, marqueeY, marqueeRot, true);
        }
    }

    void stopMarquee() {
        marquee.stop();
    }

    bool isMarqueeActive() {
        return marquee.isActive();
    }

    uintptr_t getBuffer() {
        return reinterpret_cast<uintptr_t>(canvas.getBuffer());
    }

    void clear() {
        canvas.clear();
    }
};

EMSCRIPTEN_BINDINGS(leddite_module) {
    class_<CanvasWrapper>("Canvas")
        .constructor<>()
        .function("drawSprite", &CanvasWrapper::drawSprite)
        .function("startMarquee", &CanvasWrapper::startMarquee)
        .function("updateMarquee", &CanvasWrapper::updateMarquee)
        .function("stopMarquee", &CanvasWrapper::stopMarquee)
        .function("isMarqueeActive", &CanvasWrapper::isMarqueeActive)
        .function("getBuffer", &CanvasWrapper::getBuffer)
        .function("clear", &CanvasWrapper::clear);
}
