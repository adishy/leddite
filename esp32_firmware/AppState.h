#pragma once

// Application mode — used by the main loop dispatcher in esp32_firmware.ino
enum class AppMode {
    MENU,       // Boot menu: navigate with encoder, press to select
    CLOCK_CAL,  // Clock + Calendar + Weather: NTP time (ET), date marquee, current conditions
    NETWORK,    // Network Canvas: WebSocket binary protocol (unchanged), encoder events broadcast
    TIMER,      // Visual Timer: encoder sets minutes, press starts countdown
    OCTOPUS,    // Octopus Dance: animated chibi octopus, encoder cycles colour style
    GAMES,      // Game Screensavers: auto-playing snake / life / invaders / dino
    SETTINGS,   // Settings: brightness, weather place, temperature units
    OFF,        // Screen off: LEDs blanked, short press wakes to menu
};
