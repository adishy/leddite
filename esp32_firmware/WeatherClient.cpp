#include "WeatherClient.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "OtaUpdater.h"

// Guards `latest` / `hasNew` between the fetch task and the main loop.
static portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;

void WeatherClient::begin(uint8_t placeIndex) {
    place        = (placeIndex < Places::COUNT) ? placeIndex : Places::DEFAULT_INDEX;
    placeChanged = true;      // fetch as soon as the task starts
    latest       = WeatherData();

    // 8 KB stack: TLS handshake plus JSON parsing needs considerably more than
    // the default. Priority 1 keeps it below the Arduino loop task.
    xTaskCreatePinnedToCore(&WeatherClient::taskEntry, "weather", 8192, this, 1, nullptr, 0);
    Serial.printf("[Weather] client started for %s\n", Places::get(place).name);
}

void WeatherClient::setPlace(uint8_t placeIndex) {
    if (placeIndex >= Places::COUNT) placeIndex = Places::DEFAULT_INDEX;
    if (placeIndex == place) return;

    place = placeIndex;

    // Invalidate immediately: showing the previous city's weather under a new
    // city's name would be actively misleading.
    portENTER_CRITICAL(&weatherMux);
    latest = WeatherData();          // valid = false
    hasNew = true;
    portEXIT_CRITICAL(&weatherMux);

    placeChanged = true;
    Serial.printf("[Weather] place -> %s\n", Places::get(placeIndex).name);
}

bool WeatherClient::poll(WeatherData& out) {
    bool changed = false;
    portENTER_CRITICAL(&weatherMux);
    out = latest;
    if (hasNew) { changed = true; hasNew = false; }
    portEXIT_CRITICAL(&weatherMux);
    return changed;
}

WeatherData WeatherClient::current() const {
    WeatherData copy;
    portENTER_CRITICAL(&weatherMux);
    copy = latest;
    portEXIT_CRITICAL(&weatherMux);
    return copy;
}

// ── Fetch task ────────────────────────────────────────────────────────────────

void WeatherClient::taskEntry(void* self) {
    static_cast<WeatherClient*>(self)->run();
}

void WeatherClient::run() {
    uint32_t nextFetchMs = 0;

    for (;;) {
        const uint32_t now = millis();

        if (placeChanged) {
            placeChanged = false;
            nextFetchMs  = now;      // fetch straight away
            failures     = 0;
        }

        // A TLS session costs ~40 KB of heap and an OTA write holds one open for
        // the whole download. Two at once is the one heap collision this
        // firmware can actually provoke, so the weather fetch simply waits —
        // it is on a 45-minute cadence and a minute's delay is invisible.
        if (OtaUpdater::inProgress()) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }

        if ((int32_t)(now - nextFetchMs) >= 0 && WiFi.status() == WL_CONNECTED) {
            const Place& p = Places::get(place);
            WeatherData  d;

            if (fetchOnce(p, d)) {
                portENTER_CRITICAL(&weatherMux);
                latest = d;
                hasNew = true;
                portEXIT_CRITICAL(&weatherMux);

                failures    = 0;
                nextFetchMs = millis() + REFRESH_MS;
                Serial.printf("[Weather] %s: %.1fC code=%u %s\n",
                              p.code, d.tempC10 / 10.0f, d.wmoCode,
                              d.isDay ? "day" : "night");
            } else {
                // Exponential backoff, capped — an outage must not turn into a
                // tight request loop against a free API.
                failures++;
                uint32_t wait = RETRY_BASE_MS << (failures > 5 ? 5 : (failures - 1));
                if (wait > RETRY_MAX_MS) wait = RETRY_MAX_MS;
                nextFetchMs = millis() + wait;
                Serial.printf("[Weather] fetch failed (%lu); retry in %lus\n",
                              (unsigned long)failures, (unsigned long)(wait / 1000));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool WeatherClient::fetchOnce(const Place& p, WeatherData& out) {
    char url[192];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast"
             "?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,weather_code,is_day"
             "&timezone=UTC",
             p.lat, p.lon);

    WiFiClientSecure client;
    // Open-Meteo serves public forecast data and needs no credential, so pinning
    // a root CA buys little against the cost of shipping and rotating one.
    client.setInsecure();
    client.setTimeout(HTTP_TIMEOUT_MS / 1000);

    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(client, url)) return false;

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Weather] HTTP %d\n", code);
        http.end();
        return false;
    }

    const String body = http.getString();
    http.end();

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[Weather] JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonObject cur = doc["current"];
    if (cur.isNull() || !cur["temperature_2m"].is<float>()) {
        Serial.println("[Weather] response missing 'current.temperature_2m'");
        return false;
    }

    const float tempC = cur["temperature_2m"].as<float>();
    out.tempC10 = (int16_t)lroundf(tempC * 10.0f);
    out.wmoCode = (uint8_t)(cur["weather_code"] | 0);
    out.isDay   = ((int)(cur["is_day"] | 1)) != 0;
    out.valid   = true;
    return true;
}
