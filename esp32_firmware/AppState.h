#pragma once

// Application mode — used by the main loop dispatcher in esp32_firmware.ino
enum class AppMode {
    MENU,       // Boot menu: navigate with encoder, press to select
    CLOCK_CAL,  // Clock + Calendar: NTP time (ET) alternating with date marquee
    NETWORK,    // Network Canvas: WebSocket binary protocol (unchanged), encoder events broadcast
    PATTERN,    // Pattern Slideshow: lava lamp / rainbow / pulse / sparkle
    TIMER,      // Visual Timer: encoder sets minutes, press starts countdown
    OCTOPUS,    // Octopus Dance: animated chibi octopus, encoder cycles colour style
    OFF,        // Screen off: LEDs blanked, short press wakes to menu
};
