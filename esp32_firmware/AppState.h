#pragma once

// Application mode — used by the main loop dispatcher in esp32_firmware.ino
enum class AppMode {
    MENU,       // Boot menu: navigate with encoder, press to select
    CLOCK_CAL,  // Clock + Calendar: NTP time / date alternating display
    NETWORK,    // Network Canvas: WebSocket binary protocol (unchanged), encoder events broadcast
    PATTERN,    // Pattern Slideshow: lava lamp / rainbow / pulse / sparkle
    TIMER,      // Visual Timer: encoder sets minutes, press starts countdown
};
