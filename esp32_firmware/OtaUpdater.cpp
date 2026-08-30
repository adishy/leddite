#include "OtaUpdater.h"
#include "UiController.h"
#include "Canvas.h"
#include "Draw.h"

#include <Arduino.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <esp_ota_ops.h>
#include <FastLED.h>

// ota_config.h is generated and gitignored (tools/gen-ota-config.sh). Guarding
// the include rather than requiring it means a fresh clone still compiles —
// wifi_credentials.h already costs the build one required generator step and a
// second one is a poor trade for a feature most clones will not use.
#if __has_include("ota_config.h")
  #include "ota_config.h"
  #define LEDDITE_OTA_CONFIGURED 1
#else
  #define LEDDITE_OTA_CONFIGURED 0
  #define LEDDITE_OTA_URL     ""
  #define LEDDITE_OTA_USER    ""
  #define LEDDITE_OTA_PASS    ""
  #define LEDDITE_FW_VERSION  "dev"
#endif

// Defer the core's automatic "mark this image valid" until the device has proven
// itself. This overrides the weak definition in cores/esp32/esp32-hal-misc.c,
// which is compiled as C — hence the linkage specifier. Without it initArduino()
// commits a new image before setup() has run and rollback can never trigger.
extern "C" bool verifyRollbackLater() { return true; }

static bool     s_inProgress = false;
static bool     s_confirmed  = false;
static uint32_t s_bootMs     = 0;

bool OtaUpdater::configured() { return LEDDITE_OTA_CONFIGURED; }
bool OtaUpdater::inProgress() { return s_inProgress; }
const char* OtaUpdater::version() { return LEDDITE_FW_VERSION; }

const char* OtaUpdater::runningPartition() {
    const esp_partition_t* p = esp_ota_get_running_partition();
    return p ? p->label : "?";
}

bool OtaUpdater::pendingVerify() {
    const esp_partition_t* p = esp_ota_get_running_partition();
    esp_ota_img_states_t   state;
    if (!p || esp_ota_get_state_partition(p, &state) != ESP_OK) return false;
    return state == ESP_OTA_IMG_PENDING_VERIFY;
}

void OtaUpdater::tick() {
    if (s_confirmed) return;
    if (s_bootMs == 0) s_bootMs = millis();

    // "Healthy" means it booted, joined the network and has been rendering for a
    // while — not merely that setup() returned.
    if (WiFi.status() != WL_CONNECTED) return;
    if ((uint32_t)(millis() - s_bootMs) < HEALTHY_AFTER_MS) return;

    s_confirmed = true;

    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t   state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;
    if (state != ESP_OTA_IMG_PENDING_VERIFY) return;

    esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("[OTA] image on %s marked valid after %lus healthy\n",
                  running->label, (unsigned long)(HEALTHY_AFTER_MS / 1000));
}

void OtaUpdater::run(UiController& ui, Canvas& canvas) {
    // Render straight to the panel rather than going through UiMode's 30 FPS
    // gate: this function blocks for the whole write, so the frame rate here is
    // "whenever the percentage changes" and nothing else is going to draw.
    static uint8_t pix[Draw::SIZE];
    auto paint = [&]() {
        ui.render(pix, millis());
        canvas.drawSprite(pix, 16, 16, 0, 0, 0, /*clearBefore=*/true);
        FastLED.show();
    };

    if (!configured()) {
        Serial.println("[OTA] no ota_config.h in this build — run tools/gen-ota-config.sh");
        ui.setOtaResult(false);
        paint();
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] no WiFi");
        ui.setOtaResult(false);
        paint();
        return;
    }

    s_inProgress = true;
    ui.setOtaProgress(0);
    paint();
    Serial.printf("[OTA] fetching %s (current %s)\n", LEDDITE_OTA_URL, LEDDITE_FW_VERSION);

    // The transport has to match the URL's scheme. HTTPClient uses whatever
    // client it is handed regardless of scheme, so a WiFiClientSecure pointed at
    // an http:// host opens a TLS handshake against a plain server and fails
    // with a bare -1 — and a LAN image server is plain HTTP far more often than
    // not.
    const bool useTls = (strncmp(LEDDITE_OTA_URL, "https://", 8) == 0);

    WiFiClientSecure secure;
    WiFiClient       plain;
    // For https:// the URL is a host the user configured on their own network;
    // pinning a CA would mean shipping and rotating one for a fetch that only
    // happens when somebody is standing at the device. See docs/adr/0014 for
    // why that is accepted rather than solved.
    if (useTls) secure.setInsecure();
    NetworkClient& client = useTls ? (NetworkClient&)secure : (NetworkClient&)plain;

    httpUpdate.rebootOnUpdate(false);        // we reboot ourselves, after a beat
    if (LEDDITE_OTA_PASS[0] != '\0')
        httpUpdate.setAuthorization(LEDDITE_OTA_USER, LEDDITE_OTA_PASS);

    // Redraw only when the whole percent changes. Erasing a flash sector stalls
    // the instruction cache, and repainting on every callback turns that into
    // visible flicker for no extra information.
    static uint8_t lastPct = 255;
    lastPct = 255;
    httpUpdate.onProgress([&](int done, int total) {
        const uint8_t pct = total > 0 ? (uint8_t)(((int64_t)done * 100) / total) : 0;
        if (pct == lastPct) return;
        lastPct = pct;
        ui.setOtaProgress(pct);
        paint();
        delay(1);                            // feed the idle task
    });

    const t_httpUpdate_return result =
        httpUpdate.update(client, LEDDITE_OTA_URL, LEDDITE_FW_VERSION);

    s_inProgress = false;

    switch (result) {
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] written — rebooting");
            ui.setOtaResult(true);
            paint();
            delay(1500);                     // let the OK screen be seen
            ESP.restart();
            break;

        case HTTP_UPDATE_NO_UPDATES:
            // Already current. Not a failure, but the panel has only two result
            // words and "nothing happened" is closer to ERR than to OK — say so
            // on the serial log and show the neutral one.
            Serial.println("[OTA] server reports no update available");
            ui.setOtaResult(false);
            paint();
            break;

        case HTTP_UPDATE_FAILED:
        default:
            Serial.printf("[OTA] failed (%d): %s\n",
                          httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            ui.setOtaResult(false);
            paint();
            break;
    }
}
