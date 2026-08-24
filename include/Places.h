#ifndef PLACES_H
#define PLACES_H

// Places — the fixed set of locations the weather view can show.
//
// Pure C++/stdint only — shared by the native unit tests, the WASM simulator and
// the ESP32 firmware so all three agree on indices.  The selected index is what
// gets persisted to NVS, so APPENDING to this table is safe but REORDERING it
// will silently change a saved preference.
//
// `code` is what renders in the settings submenu and flashes on the weather
// view.  SmallTextRenderer advances 4px per letter and a row is 16px wide, so
// codes of 4 characters or fewer display without scrolling — keep them short.

#include <stdint.h>

struct Place {
    const char* code;   // <=4 chars renders without scrolling
    const char* name;   // full name, for logs and docs
    float       lat;
    float       lon;
};

class Places {
public:
    static const uint8_t COUNT          = 7;
    static const uint8_t DEFAULT_INDEX  = 1;   // Cambridge, MA 02141

    // Returns the place at `index`, clamped into range so a corrupt or
    // out-of-date NVS value can never index out of bounds.
    static const Place& get(uint8_t index);

private:
    static const Place TABLE[COUNT];
};

#endif // PLACES_H
