#include "Places.h"

// Coordinates are city-centre (East Cambridge for the 02141 default).
// Open-Meteo snaps to its own forecast grid, so additional precision is wasted.
const Place Places::TABLE[Places::COUNT] = {
    { "NYC",  "New York, NY",           40.7128f,  -74.0060f },
    { "CAMB", "Cambridge, MA 02141",    42.3706f,  -71.0870f },
    { "SFO",  "San Francisco, CA",      37.7749f, -122.4194f },
    { "SLL",  "Salalah, Oman",          17.0151f,   54.0924f },
    { "MCT",  "Muscat, Oman",           23.5880f,   58.3829f },
    { "AMH",  "Amherst, MA",            42.3732f,  -72.5199f },
    { "TVM",  "Trivandrum, Kerala",      8.5241f,   76.9366f },
};

const Place& Places::get(uint8_t index) {
    if (index >= COUNT) index = DEFAULT_INDEX;
    return TABLE[index];
}
