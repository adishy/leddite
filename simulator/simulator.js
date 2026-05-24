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
let wasmModule = null;
let ws = null;  // kept at module scope so encoder buttons can send on it

// ── WASM init ─────────────────────────────────────────────────────────────────
Module.onRuntimeInitialized = () => {
    wasmModule = Module;
    ledditeCanvas = new wasmModule.Canvas();
    console.log("WASM logic initialised");
    initGrid();
    connect();
    animate();
};

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
    if (!ledditeCanvas) return;
    const bufferPtr  = ledditeCanvas.getBuffer();
    const bufferSize = WIDTH * HEIGHT * 3;
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

// ── Marquee animation loop ────────────────────────────────────────────────────
function animate() {
    if (ledditeCanvas && ledditeCanvas.isMarqueeActive()) {
        ledditeCanvas.updateMarquee(Date.now());
        updateDOM();
    }
    requestAnimationFrame(animate);
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

btnCCW.addEventListener('click', () => {
    flashBtn(btnCCW);
    sendEncoderEvent({ type: 'encoder', delta: -1 });
});

btnCW.addEventListener('click', () => {
    flashBtn(btnCW);
    sendEncoderEvent({ type: 'encoder', delta: 1 });
});

// Press: mousedown → "pressed" event, mouseup → "released" event
btnPress.addEventListener('mousedown', () => {
    flashBtn(btnPress, 500);
    sendEncoderEvent({ type: 'encoder', button: 'pressed' });
});
btnPress.addEventListener('mouseup', () => {
    sendEncoderEvent({ type: 'encoder', button: 'released' });
});

// Long-press simulation (L key only — simulates 3 s hold)
function simulateLongPress() {
    flashBtn(btnPress, 600);
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
            sendEncoderEvent({ type: 'encoder', button: 'pressed' });
            break;
        case 'l':
        case 'L':          simulateLongPress(); break;
        case 'c':
        case 'C':          clearDisplay();      break;
    }
});

document.addEventListener('keyup', (e) => {
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

document.getElementById('clear-btn').addEventListener('click', clearDisplay);

// ── Public API (used by test_suite.py inject path if needed) ──────────────────
window.handleLedditePacket = handleBinary;
