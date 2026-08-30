#pragma once

#include <stdint.h>

class UiController;
class Canvas;

// OtaUpdater — over-the-air firmware updates, pulled on demand.
//
// WHY A PULL, TRIGGERED BY THE ENCODER
// ------------------------------------
// The three candidates were measured against this firmware on esp32 core 3.3.8
// (baseline 1,184,240 B of a 1,310,720 B app slot):
//
//   ArduinoOTA (+ mDNS)              +54,260 B flash  +4,712 B RAM
//   WebServer upload form + Update   +31,400 B flash    +624 B RAM
//   HTTPUpdate pull                  +13,936 B flash    +384 B RAM
//
// HTTPUpdate is both the cheapest and the only one that leaves nothing
// listening. ArduinoOTA and the upload form each add a network service that
// accepts firmware from anyone on the LAN for the entire uptime of the device;
// this one reaches out exactly when somebody standing at the panel selects
// Settings -> UPDATE -> YES. On a home LAN that beats any password on an open
// socket, which is why the HTTP Basic credentials here are a guard against a
// mistake rather than against an attacker.
//
// The TLS stack is already paid for: WeatherClient links WiFiClientSecure and
// HTTPClient regardless, so an https:// URL costs nothing extra here.
//
// PARTITIONS
// ----------
// OTA needs two app slots. `default.csv` already has them (app0 @0x10000 and
// app1 @0x150000, 0x140000 each) — the "90% of flash" figure in CLAUDE.md was
// always 90% of *one slot*, not of the whole chip. The build nevertheless moves
// to `min_spiffs`, which keeps two slots and raises each to 0x1E0000, taking
// usage from ~91% to ~61%. Nothing in this repo uses SPIFFS or LittleFS, and
// `nvs` stays at 0x9000/0x5000 in both schemes, so the saved brightness, place
// and unit survive the switch.
//
// **The first flash after this change must be over USB.** A partition table is
// not part of an OTA payload, so a device still running the `default` layout
// would write a min_spiffs image into a slot at the wrong offset.
//
// ROLLBACK
// --------
// The core enables CONFIG_APP_ROLLBACK_ENABLE and, by default, marks a freshly
// written image valid inside initArduino() — *before* setup() runs — so an image
// that bricks itself on boot is already committed by the time it fails. This
// class overrides the core's weak verifyRollbackLater() hook to defer that, and
// calls confirmBootHealthy() only once the device has actually joined WiFi and
// rendered frames for HEALTHY_AFTER_MS. A bad image reverts to the previous slot
// on the next reboot instead of needing a cable.

class OtaUpdater {
public:
    // How long the new image has to keep running before it is marked good.
    static const uint32_t HEALTHY_AFTER_MS = 30000;

    // True when this build has an ota_config.h to fetch from.
    static bool configured();

    // The version string this image reports, both in the boot banner and as the
    // x-ESP32-version header. Printing it at boot is what makes an OTA
    // verifiable: without it, a successful update and a silent no-op look
    // exactly alike on the serial log.
    static const char* version();

    // Which app slot is running ("app0"/"app1"), and whether this image is still
    // on probation — i.e. written by OTA and not yet confirmed healthy.
    static const char* runningPartition();
    static bool        pendingVerify();

    // Call every loop(). Marks a pending image valid once the device has proven
    // it can boot, connect and render. Cheap and idempotent after that.
    static void tick();

    // Blocking: fetches and writes the new image, driving the panel's progress
    // screen through `ui` as bytes land. Reboots on success.
    static void run(UiController& ui, Canvas& canvas);

    // True while a flash write is in flight — WeatherClient uses this to stay
    // off the air. Two concurrent TLS sessions cost ~40 KB of heap each and this
    // device has ~250 KB free; the weather fetch is the one thing that could
    // collide with an update, and it is entirely skippable for a minute.
    static bool inProgress();
};
