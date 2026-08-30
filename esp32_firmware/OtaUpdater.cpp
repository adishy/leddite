#include "OtaUpdater.h"
#include "UiController.h"
#include "Canvas.h"
#include "Draw.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <FastLED.h>

// Bump this when you cut an image. Overridable from the build so CI can stamp
// one without editing a file:
//   --build-property "compiler.cpp.extra_flags=-DLEDDITE_FW_VERSION=\"2.3.0\""
#ifndef LEDDITE_FW_VERSION
  #define LEDDITE_FW_VERSION "2.2.0"
#endif

// Defer the core's automatic "mark this image valid" until the device has
// proven itself. This overrides the weak definition in
// cores/esp32/esp32-hal-misc.c, which is compiled as C — hence the linkage
// specifier. Without it initArduino() commits a new image before setup() has
// run and rollback can never trigger.
extern "C" bool verifyRollbackLater() { return true; }

static bool       s_inProgress = false;
static bool       s_confirmed  = false;
static uint32_t   s_bootMs     = 0;
static WebServer* s_server     = nullptr;
static uint32_t   s_expected   = 0;   // Content-Length, for the panel's bar
static uint8_t    s_lastPct    = 255;

bool OtaUpdater::inProgress() { return s_inProgress; }
bool OtaUpdater::windowOpen() { return s_server != nullptr; }
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

    // "Healthy" means it booted, joined the network and has been rendering for
    // a while — not merely that setup() returned.
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

// ── The upload page ──────────────────────────────────────────────────────────
// Served from flash as one string. No CDN, no framework: the whole point of
// this flow is that it works with nothing but the device and a browser, and a
// page that needs to fetch anything would defeat that on exactly the network
// where you most need it.
static const char PAGE[] PROGMEM = R"HTML(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Leddite firmware</title><style>
:root{color-scheme:dark}
body{margin:0;font:15px/1.5 system-ui,sans-serif;background:#12141a;color:#e7e9ee;
display:flex;min-height:100vh;align-items:center;justify-content:center}
.c{width:min(420px,92vw)}
h1{font-size:19px;margin:0 0 4px}
.m{color:#8b93a5;font-size:13px;margin-bottom:18px}
label{display:block;border:1px dashed #39405280;border-radius:10px;padding:26px 16px;
text-align:center;cursor:pointer;background:#191c24}
label:hover{border-color:#ff78a0;background:#1d2029}
input[type=file]{display:none}
#n{margin-top:12px;font-size:13px;color:#8b93a5;min-height:19px;word-break:break-all}
button{width:100%;margin-top:14px;padding:11px;border:0;border-radius:9px;
background:#ff78a0;color:#12141a;font:600 15px system-ui;cursor:pointer}
button:disabled{opacity:.4;cursor:default}
#t{height:8px;border-radius:5px;background:#262a35;margin-top:16px;overflow:hidden;display:none}
#b{height:100%;width:0;background:#ff78a0;transition:width .15s}
#s{margin-top:10px;font-size:13px;min-height:19px}
.ok{color:#5aeb78}.err{color:#ff5f52}
</style>
<div class=c>
<h1>Leddite firmware</h1>
<div class=m>Running <b id=v></b> on <b id=p></b>. Upload a
<code>.bin</code> to the other slot.</div>
<label for=f>Choose or drop a firmware image<div id=n></div></label>
<input type=file id=f accept=".bin">
<button id=g disabled>Install</button>
<div id=t><div id=b></div></div>
<div id=s></div>
</div>
<script>
const $=i=>document.getElementById(i);
fetch('/info').then(r=>r.json()).then(j=>{$('v').textContent=j.version;$('p').textContent=j.slot})
 .catch(()=>{});
let file=null;
const pick=f=>{file=f;$('n').textContent=f?f.name+' — '+(f.size/1024).toFixed(0)+' KB':'';
 $('g').disabled=!f};
$('f').onchange=e=>pick(e.target.files[0]);
document.ondragover=e=>e.preventDefault();
document.ondrop=e=>{e.preventDefault();pick(e.dataTransfer.files[0])};
$('g').onclick=()=>{
 if(!file)return;
 $('g').disabled=true;$('t').style.display='block';$('s').className='';
 $('s').textContent='Uploading… do not close this tab.';
 const d=new FormData();d.append('f',file,file.name);
 const x=new XMLHttpRequest();
 x.upload.onprogress=e=>{if(e.lengthComputable)$('b').style.width=(e.loaded/e.total*100)+'%'};
 x.onload=()=>{
  const ok=x.status===200&&x.responseText.indexOf('OK')===0;
  $('s').className=ok?'ok':'err';
  $('s').textContent=ok?'Installed. The panel is rebooting — check the boot banner for the new version.'
                       :'Failed: '+(x.responseText||x.status);
  if(!ok)$('g').disabled=false;
 };
 x.onerror=()=>{$('s').className='err';$('s').textContent='Connection lost during upload.';
  $('g').disabled=false};
 x.open('POST','/update');x.send(d);
};
</script>)HTML";

// ── Upload handling ──────────────────────────────────────────────────────────

static void paintPanel(UiController& ui, Canvas& canvas) {
    static uint8_t pix[Draw::SIZE];
    ui.render(pix, millis());
    canvas.drawSprite(pix, 16, 16, 0, 0, 0, /*clearBefore=*/true);
    FastLED.show();
}

// Captured for the upload callbacks, which WebServer gives no user pointer.
static UiController* s_ui     = nullptr;
static Canvas*       s_canvas = nullptr;

static void onUploadData() {
    HTTPUpload& up = s_server->upload();

    if (up.status == UPLOAD_FILE_START) {
        // Content-Length includes the multipart envelope — a few hundred bytes
        // against ~1.2 MB. Close enough for a progress bar, and the browser
        // shows the exact figure anyway.
        s_expected   = (uint32_t)s_server->header("Content-Length").toInt();
        s_lastPct    = 255;
        s_inProgress = true;
        Serial.printf("[OTA] upload starting: %s (%lu B)\n",
                      up.filename.c_str(), (unsigned long)s_expected);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
            s_inProgress = false;
        }
        return;
    }

    if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.isRunning() && Update.write(up.buf, up.currentSize) != up.currentSize)
            Update.printError(Serial);

        // Redraw only when the whole percent changes. Erasing a flash sector
        // stalls the instruction cache, and repainting on every chunk turns
        // that into visible flicker for no extra information.
        if (s_expected > 0 && s_ui && s_canvas) {
            const uint8_t pct =
                (uint8_t)(((uint64_t)up.totalSize * 100) / s_expected);
            if (pct != s_lastPct && pct <= 100) {
                s_lastPct = pct;
                s_ui->setOtaProgress(pct);
                paintPanel(*s_ui, *s_canvas);
            }
        }
        return;
    }

    if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
        if (up.status == UPLOAD_FILE_ABORTED) {
            Update.abort();
            Serial.println("[OTA] upload aborted by client");
        }
        s_inProgress = false;
    }
}

static void onUploadDone() {
    const bool ok = Update.end(true);
    if (!ok) Update.printError(Serial);

    s_server->sendHeader("Connection", "close");
    s_server->send(ok ? 200 : 500, "text/plain",
                   ok ? "OK" : Update.errorString());

    if (s_ui) s_ui->setOtaResult(ok);
    if (s_ui && s_canvas) paintPanel(*s_ui, *s_canvas);

    if (ok) {
        Serial.println("[OTA] written — rebooting");
        delay(1500);              // let the OK screen be seen and the reply flush
        ESP.restart();
    }
}

static void openWindow(UiController& ui, Canvas& canvas) {
    if (s_server) return;

    s_ui     = &ui;
    s_canvas = &canvas;
    s_server = new WebServer(OtaUpdater::PORT);

    // WebServer discards headers it was not told to keep, and the upload
    // progress needs Content-Length.
    static const char* keep[] = { "Content-Length" };
    s_server->collectHeaders(keep, 1);

    s_server->on("/", HTTP_GET, []() {
        s_server->send_P(200, "text/html", PAGE);
    });
    s_server->on("/info", HTTP_GET, []() {
        String j = String("{\"version\":\"") + OtaUpdater::version() +
                   "\",\"slot\":\"" + OtaUpdater::runningPartition() + "\"}";
        s_server->send(200, "application/json", j);
    });
    s_server->on("/update", HTTP_POST, onUploadDone, onUploadData);
    s_server->onNotFound([]() {
        s_server->sendHeader("Location", "/");
        s_server->send(302, "text/plain", "");
    });

    s_server->begin();
    Serial.printf("[OTA] upload window open — http://%s/ (closes in %lus)\n",
                  WiFi.localIP().toString().c_str(),
                  (unsigned long)(UiController::OTA_WINDOW_MS / 1000));
}

static void closeWindow() {
    if (!s_server) return;
    s_server->stop();
    delete s_server;
    s_server = nullptr;
    s_ui     = nullptr;
    s_canvas = nullptr;
    Serial.println("[OTA] upload window closed");
}

void OtaUpdater::service(UiController& ui, Canvas& canvas) {
    if (ui.otaRequested()) {
        ui.clearOtaRequest();
        openWindow(ui, canvas);
    }

    const UiController::OtaPhase phase = ui.otaPhase();
    const bool wanted = (phase == UiController::OtaPhase::WAITING ||
                         phase == UiController::OtaPhase::RUNNING);

    if (!wanted) { closeWindow(); return; }
    if (s_server) s_server->handleClient();
}
