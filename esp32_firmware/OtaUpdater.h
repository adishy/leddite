#pragma once

#include <stdint.h>

class UiController;
class Canvas;

// OtaUpdater — over-the-air firmware updates, uploaded to the device.
//
// THE FLOW
// --------
// At the panel: Settings -> UPDATE -> YES. The device opens an HTTP server on
// port 80 and scrolls its own URL across the display. You browse to that URL,
// pick a .bin, and the browser POSTs it straight into the spare app slot. The
// window closes on upload, on cancel, or after UiController::OTA_WINDOW_MS.
//
// WHY THIS, AND NOT THE PULL IT REPLACES
// --------------------------------------
// The original design fetched an image from a URL baked in at compile time.
// Three things were wrong with it in practice:
//
//   1. It needs a server. Somebody has to run one, on a machine the device can
//      reach, and keep it running.
//   2. It needs the device to reach *you*. On the network this was built on it
//      could not: the panel reaches its gateway in 8 ms and 1.1.1.1 in 20 ms,
//      and times out against the build host every time. Firmware cannot fix a
//      routing problem, and the pull path was never once exercised end to end.
//   3. The URL is compile-time, so changing where updates come from needs the
//      cable the feature exists to avoid.
//
// Uploading inverts all three. The browser already holds the file, the
// connection runs browser -> device (the direction that works here, as the
// WebSocket canvas on :81 has always demonstrated), and there is nothing to
// configure: the address you need is on the panel in front of you.
//
// "BUT SOMETHING IS LISTENING NOW"
// --------------------------------
// docs/adr/0014 rejected an upload form precisely because it listens. That
// objection was to a server running for the device's whole uptime, accepting
// firmware from anyone on the LAN. This one exists only inside a window a
// person opened by turning a knob and pressing it, and closes itself after five
// minutes. Physical presence remains the authentication factor, which is the
// property the ADR actually cared about.
//
// PARTITIONS
// ----------
// OTA needs two app slots; `min_spiffs` gives two of 0x1E0000, taking this
// build to ~61% of a slot. `nvs` stays at 0x9000/0x5000, so saved brightness,
// place and unit survive. **The first flash after a partition change must be
// over USB** — a partition table is not part of an OTA payload.
//
// ROLLBACK
// --------
// The core enables CONFIG_APP_ROLLBACK_ENABLE and marks a freshly written image
// valid inside initArduino(), *before* setup() runs — so an image that bricks
// itself on boot is already committed by the time it fails. This class
// overrides the weak verifyRollbackLater() hook to defer that, and confirms the
// image only once the device has joined WiFi and run for HEALTHY_AFTER_MS. A
// bad image reverts to the previous slot on the next reboot, with no cable.

class OtaUpdater {
public:
    // How long the new image has to keep running before it is marked good.
    static const uint32_t HEALTHY_AFTER_MS = 30000;

    // The port the upload page is served on. 80 so the URL on the panel needs
    // no ":port" suffix — that is four more glyphs to read off a 16px display
    // and four more chances to mistype it.
    static const uint16_t PORT = 80;

    // The version string this image reports, printed in the boot banner and
    // shown on the upload page. Printing it is what makes an update verifiable:
    // without it, a real update and a silent no-op look identical.
    static const char* version();

    // Which app slot is running ("app0"/"app1"), and whether this image is
    // still on probation — written by OTA and not yet confirmed healthy.
    static const char* runningPartition();
    static bool        pendingVerify();

    // Call every loop(). Marks a pending image valid once the device has proven
    // it can boot, connect and render. Cheap and idempotent after that.
    static void tick();

    // Call every loop(). Opens the upload window when the panel asks for it,
    // pumps the HTTP server while it is open, and closes it again as soon as
    // the UI leaves the waiting/running phases. Non-blocking; the upload itself
    // runs inside handleClient().
    static void service(UiController& ui, Canvas& canvas);

    // True while a flash write is in flight — WeatherClient uses this to stay
    // off the air. Two concurrent TLS sessions cost ~40 KB of heap each and
    // this device has ~250 KB free; the weather fetch is the one thing that
    // could collide with an update, and it is entirely skippable for a minute.
    static bool inProgress();

    // True while the upload window is open.
    static bool windowOpen();
};
