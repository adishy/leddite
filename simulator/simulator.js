/**
 * Leddite V2 Simulator — WASM Powered + Encoder UI
 *
 * Rendering:  100% logic parity with ESP32 via Emscripten WASM (Canvas, Transformer, MarqueeEngine)
 * Transport:  WebSocket relay at ws://localhost:8765
 * Encoder:    UI buttons + keyboard shortcuts emit JSON encoder events to all WS peers
 */

const WIDTH = 16;
const HEIGHT = 16;
const ledElements = [];
let ledditeCanvas = null;
let deviceUI = null;
let wasmModule = null;
let ws = null;  // kept at module scope so encoder buttons can send on it

// 'network' renders frames arriving over the WebSocket protocol.
// 'device'  runs the real firmware UI (UiController) compiled to WASM, so the
//           games/settings/weather screens here are the same code the ESP32
//           executes rather than a JS reimplementation (docs/adr/0009).
let renderMode = 'network';

// Mirrors UiController::Screen — for the on-page readout only.
const SCREEN_NAMES = [
    'GAMES MENU', 'GAME PLAYING', 'SETTINGS MENU',
    'BRIGHTNESS', 'PLACES', 'UNITS', 'WEATHER', 'IP', 'UPDATE',
];

// ── WASM init ─────────────────────────────────────────────────────────────────
// The module is built with MODULARIZE=1, so leddite_wasm.js defines a factory
// instead of assigning a global `Module`. Awaiting it removes the old
// `var Module = {}` pre-declaration requirement, a missing instance of which
// previously left the display black with no error.
createLedditeModule().then((mod) => {
    wasmModule    = mod;
    ledditeCanvas = new mod.Canvas();
    deviceUI      = new mod.DeviceUI();

    // Seed a plausible reading so the weather screen shows something before any
    // real WeatherClient data exists (12.3 C, clear, daytime).
    deviceUI.setWeather(123, 0, true, true);

    // Stand in for the firmware's WiFi so Settings -> IP shows a plausible
    // address rather than 0.0.0.0.
    deviceUI.setIpAddress(192, 168, 0, 113);

    console.log('WASM logic initialised (Canvas + DeviceUI)');
    initGrid();
    connect();
    animate();
}).catch((err) => {
    console.error('WASM failed to initialise:', err);
    const s = document.getElementById('status-text');
    if (s) { s.textContent = 'WASM failed to load'; s.style.color = '#f44'; }
});

// ── LED grid ──────────────────────────────────────────────────────────────────
function initGrid() {
    const grid = document.getElementById('led-grid');
    grid.innerHTML = '';
    for (let i = 0; i < WIDTH * HEIGHT; i++) {
        const led = document.createElement('div');
        led.className = 'led';
        grid.appendChild(led);
        ledElements.push(led);
    }
}

function updateDOM() {
    const source = (renderMode === 'device') ? deviceUI : ledditeCanvas;
    if (!source) return;

    const bufferPtr  = source.getBuffer();
    const bufferSize = WIDTH * HEIGHT * 3;
    // Re-created each call: ALLOW_MEMORY_GROWTH can detach the old ArrayBuffer.
    const buffer     = new Uint8Array(wasmModule.HEAPU8.buffer, bufferPtr, bufferSize);

    for (let i = 0; i < WIDTH * HEIGHT; i++) {
        const r = buffer[i * 3];
        const g = buffer[i * 3 + 1];
        const b = buffer[i * 3 + 2];
        const el = ledElements[i];
        el.style.backgroundColor = `rgb(${r},${g},${b})`;
        el.style.boxShadow = (r | g | b)
            ? `0 0 6px rgb(${r},${g},${b})`
            : 'none';
    }
}

// ── Binary frame handler ──────────────────────────────────────────────────────
function handleBinary(data) {
    if (!ledditeCanvas || data.length < 8) return;

    const version  = data[0];
    const flags    = data[1];
    const width    = data[2];
    const height   = data[3];
    const x_offset = data[4] > 127 ? data[4] - 256 : data[4];
    const y_offset = data[5] > 127 ? data[5] - 256 : data[5];
    const rotation = data[6];
    const brightness = data[7];

    const pixels     = data.slice(8);
    const clearBefore = (flags & 0x01) !== 0;
    const isMarquee   = (flags & 0x04) !== 0;

    const pixelPtr = wasmModule._malloc(pixels.length);
    wasmModule.HEAPU8.set(pixels, pixelPtr);

    if (isMarquee) {
        ledditeCanvas.startMarquee(pixelPtr, width, height, rotation, y_offset, Date.now());
    } else {
        ledditeCanvas.stopMarquee();
        ledditeCanvas.drawSprite(pixelPtr, width, height, x_offset, y_offset, rotation, clearBefore);
    }

    wasmModule._free(pixelPtr);
    updateDOM();

    document.getElementById('sprite-info').textContent =
        `${width}×${height} @ (${x_offset},${y_offset}) rot=${rotation * 90}° ${isMarquee ? '[marquee]' : ''}`;
}

// ── Animation loop ────────────────────────────────────────────────────────────
function animate() {
    if (renderMode === 'device') {
        if (deviceUI) {
            // The browser supplies nowMs — the same injection the unit tests use.
            deviceUI.tick(Date.now() >>> 0);
            updateDOM();
            const el = document.getElementById('device-screen');
            if (el) el.textContent = SCREEN_NAMES[deviceUI.screen()] || '—';
        }
    } else if (ledditeCanvas && ledditeCanvas.isMarqueeActive()) {
        ledditeCanvas.updateMarquee(Date.now());
        updateDOM();
    }
    requestAnimationFrame(animate);
}

// ── Render-mode switching ─────────────────────────────────────────────────────
function setRenderMode(mode) {
    renderMode = mode;
    document.getElementById('mode-network').classList.toggle('active', mode === 'network');
    document.getElementById('mode-device').classList.toggle('active',  mode === 'device');
    document.getElementById('device-controls').style.display = (mode === 'device') ? '' : 'none';
    if (mode === 'device' && deviceUI) deviceUI.enterGames(Date.now() >>> 0);
    updateDOM();
}

document.getElementById('mode-network').addEventListener('click', () => setRenderMode('network'));
document.getElementById('mode-device').addEventListener('click',  () => setRenderMode('device'));

document.getElementById('dev-games').addEventListener('click', () => {
    if (deviceUI) { setRenderMode('device'); deviceUI.enterGames(Date.now() >>> 0); }
});
document.getElementById('dev-settings').addEventListener('click', () => {
    if (deviceUI) { setRenderMode('device'); deviceUI.enterSettings(Date.now() >>> 0); }
});
document.getElementById('dev-weather').addEventListener('click', () => {
    if (deviceUI) { setRenderMode('device'); deviceUI.enterWeather(Date.now() >>> 0); }
});

// Routes an encoder gesture into the WASM UI. Returns true when it was consumed,
// so the existing network-mode behaviour is left untouched.
function deviceUIEncoder(kind, delta) {
    if (renderMode !== 'device' || !deviceUI) return false;
    const now = Date.now() >>> 0;
    // encoderTurn, not turn: the browser control stands in for the physical
    // knob, so it must inherit the same inversion and granularity. A simulator
    // with a nicer knob than the device is a simulator you cannot trust for
    // feel (docs/adr/0010, docs/adr/0016).
    if (kind === 'turn')       deviceUI.encoderTurn(delta, now);
    else if (kind === 'press') deviceUI.press(now);
    else if (kind === 'long') {
        // true means "nowhere further up" — the device would return to its main
        // menu here, so the simulator drops back to the games list.
        if (deviceUI.longPress(now)) deviceUI.enterGames(now);
    }
    updateDOM();
    return true;
}

// ── WebSocket ─────────────────────────────────────────────────────────────────
function connect() {
    ws = new WebSocket('ws://localhost:8765');
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => {
        document.getElementById('status-text').textContent = 'Connected';
        document.getElementById('status-text').style.color = '#4f4';
        console.log('[WS] Connected to relay server');
    };

    ws.onmessage = (event) => {
        if (event.data instanceof ArrayBuffer) {
            handleBinary(new Uint8Array(event.data));
        } else if (typeof event.data === 'string') {
            // JSON encoder event relayed from another client (e.g. hardware ESP32 or test suite)
            try {
                const ev = JSON.parse(event.data);
                if (ev.type === 'encoder') {
                    logEncoderEvent('← ' + JSON.stringify(ev));
                }
            } catch (_) {}
        }
    };

    ws.onclose = () => {
        document.getElementById('status-text').textContent = 'Disconnected — reconnecting…';
        document.getElementById('status-text').style.color = '#f44';
        ws = null;
        setTimeout(connect, 2000);
    };

    ws.onerror = (err) => {
        console.warn('[WS] Error:', err);
    };
}

// ── Encoder helpers ───────────────────────────────────────────────────────────
const encoderLog = document.getElementById('encoder-log');
let encoderLogLines = [];

function logEncoderEvent(msg) {
    const ts = new Date().toLocaleTimeString('en-US', { hour12: false,
        hour: '2-digit', minute: '2-digit', second: '2-digit' });
    encoderLogLines.push(`[${ts}] ${msg}`);
    if (encoderLogLines.length > 12) encoderLogLines.shift();
    encoderLog.textContent = encoderLogLines.join('\n');
    encoderLog.scrollTop = encoderLog.scrollHeight;
}

function sendEncoderEvent(payload) {
    const json = JSON.stringify(payload);
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(json);
        logEncoderEvent('→ ' + json);
    } else {
        logEncoderEvent('✕ not connected — ' + json);
    }
}

function flashBtn(el, ms = 120) {
    el.classList.add('pressed');
    setTimeout(() => el.classList.remove('pressed'), ms);
}

// ── Encoder button wiring ─────────────────────────────────────────────────────
const btnCCW   = document.getElementById('enc-ccw');
const btnPress = document.getElementById('enc-press');
const btnCW    = document.getElementById('enc-cw');

// In device mode the gesture drives the WASM UiController directly; in network
// mode it is broadcast as JSON exactly as before.
btnCCW.addEventListener('click', () => {
    flashBtn(btnCCW);
    if (deviceUIEncoder('turn', -1)) return;
    sendEncoderEvent({ type: 'encoder', delta: -1 });
});

btnCW.addEventListener('click', () => {
    flashBtn(btnCW);
    if (deviceUIEncoder('turn', 1)) return;
    sendEncoderEvent({ type: 'encoder', delta: 1 });
});

// Press: mousedown → "pressed" event, mouseup → "released" event
btnPress.addEventListener('mousedown', () => {
    flashBtn(btnPress, 500);
    if (deviceUIEncoder('press')) return;
    sendEncoderEvent({ type: 'encoder', button: 'pressed' });
});
btnPress.addEventListener('mouseup', () => {
    if (renderMode === 'device') return;
    sendEncoderEvent({ type: 'encoder', button: 'released' });
});

// Long-press simulation (L key only — simulates 3 s hold)
function simulateLongPress() {
    flashBtn(btnPress, 600);
    if (deviceUIEncoder('long')) {
        logEncoderEvent('  (long-press → up one level in the device UI)');
        return;
    }
    sendEncoderEvent({ type: 'encoder', longPress: true });
    logEncoderEvent('  (long-press simulated — triggers back-to-menu on hardware)');
}

// ── Keyboard shortcuts ────────────────────────────────────────────────────────
document.addEventListener('keydown', (e) => {
    // Ignore when typing in an input
    if (e.target.tagName === 'INPUT') return;
    switch (e.key) {
        case 'ArrowLeft':  e.preventDefault(); btnCCW.click();         break;
        case 'ArrowRight': e.preventDefault(); btnCW.click();          break;
        case 'Enter':
        case ' ':          e.preventDefault();
            flashBtn(btnPress, 500);
            if (deviceUIEncoder('press')) break;
            sendEncoderEvent({ type: 'encoder', button: 'pressed' });
            break;
        case 'l':
        case 'L':          simulateLongPress(); break;
        case 'c':
        case 'C':          clearDisplay();      break;
    }
});

document.addEventListener('keyup', (e) => {
    if (renderMode === 'device') return;
    if (e.key === 'Enter' || e.key === ' ') {
        sendEncoderEvent({ type: 'encoder', button: 'released' });
    }
});

// ── Clear button ──────────────────────────────────────────────────────────────
function clearDisplay() {
    if (ledditeCanvas) {
        ledditeCanvas.stopMarquee();
        ledditeCanvas.clear();
        updateDOM();
        document.getElementById('sprite-info').textContent = '—';
    }
}

// ── OTA guide: "show me that screen" ──────────────────────────────────────────
// Drives the real UiController rather than showing a picture of it, so what the
// guide points at is the same state machine the ESP32 runs (docs/adr/0010). If
// the screen ever changes, this walks to the new one.
const SETTINGS_ROW = { ip: 'IP_VIEW', update: 'UPDATE' };
const SCREEN_ID    = { IP_VIEW: 7, UPDATE: 8 };

function showSettingsScreen(which) {
    if (!deviceUI) return;
    setRenderMode('device');

    const want = SCREEN_ID[SETTINGS_ROW[which]];
    const now  = () => Date.now() >>> 0;

    // Walk the settings list pressing each row until the target opens, backing
    // out of any row that is not it. Bounded so a renamed row cannot spin.
    deviceUI.enterSettings(now());
    for (let i = 0; i < 8; i++) {
        deviceUI.press(now());
        if (deviceUI.screen() === want) break;
        if (deviceUI.screen() !== 2) deviceUI.longPress(now());   // 2 = SETTINGS_MENU
        deviceUI.turn(1, now());
    }
    document.getElementById('matrix-container')
            .scrollIntoView({ behavior: 'smooth', block: 'center' });
}

document.querySelectorAll('.ota-try').forEach((el) => {
    el.addEventListener('click', () => showSettingsScreen(el.dataset.goto));
});

document.getElementById('clear-btn').addEventListener('click', clearDisplay);

// ── Public API (used by test_suite.py inject path if needed) ──────────────────
window.handleLedditePacket = handleBinary;
