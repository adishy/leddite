#pragma once

#include "WeatherView.h"     // WeatherData
#include "Places.h"
#include <stdint.h>

// WeatherClient — fetches current conditions from Open-Meteo.
//
// Open-Meteo needs no API key and takes lat/lon directly, so there is no
// credential on the device and no geocoding step. Only the three fields the
// display uses are requested, keeping the response around 250 bytes.
//
//   https://api.open-meteo.com/v1/forecast
//     ?latitude=..&longitude=..&current=temperature_2m,weather_code,is_day
//
// Always fetched in CELSIUS; conversion to Fahrenheit happens in WeatherView.
// That makes the units toggle instant, works on cached data, and avoids a
// refetch just to change how a number is displayed.
//
// THREADING
// The fetch runs on its own FreeRTOS task. A blocking HTTPS request on the main
// loop would stall whatever animation is on screen for seconds. The task writes
// into a small shared struct guarded by a spinlock; the loop polls it.
//
// RATE LIMITING
// One request per REFRESH_MS (45 minutes) — about 32/day, far inside
// Open-Meteo's allowance. Note the clock/date/weather views rotate every 10 s,
// so refreshing on view entry would be ~8,600 requests/day; the cached reading
// is served instead. Failures back off exponentially so an outage does not
// hammer the API.

class WeatherClient {
public:
    static const uint32_t REFRESH_MS      = 45UL * 60UL * 1000UL;  // 45 min
    static const uint32_t RETRY_BASE_MS   = 30UL * 1000UL;         // first retry
    static const uint32_t RETRY_MAX_MS    = 15UL * 60UL * 1000UL;  // backoff ceiling
    static const uint16_t HTTP_TIMEOUT_MS = 8000;

    void begin(uint8_t placeIndex);

    // Changing place invalidates the cached reading and triggers an immediate
    // fetch — the user asked to see somewhere else, so stale data is wrong.
    void setPlace(uint8_t placeIndex);

    // Copies the latest reading out. Returns true if it changed since last poll.
    bool poll(WeatherData& out);

    // Current reading regardless of change, for first render.
    WeatherData current() const;

private:
    static void taskEntry(void* self);
    void        run();
    bool        fetchOnce(const Place& p, WeatherData& out);

    volatile uint8_t  place        = Places::DEFAULT_INDEX;
    volatile bool     placeChanged = false;
    volatile bool     hasNew       = false;
    WeatherData       latest;
    uint32_t          failures     = 0;
};
