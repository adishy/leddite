/**
 * Leddite V2 Simulator - WASM Powered
 * 100% Logic Parity with ESP32 via Emscripten
 */

const WIDTH = 16;
const HEIGHT = 16;
const ledElements = [];
let ledditeCanvas = null;
let wasmModule = null;

// Initialize WASM
Module.onRuntimeInitialized = () => {
    wasmModule = Module;
    ledditeCanvas = new wasmModule.Canvas();
    console.log("WASM Logic Initialized");
    initGrid();
    connect();
    animate();
};

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
    
    // Get pointer to the internal buffer
    const bufferPtr = ledditeCanvas.getBuffer();
    const bufferSize = WIDTH * HEIGHT * 3; // CRGB is 3 bytes
    const buffer = new Uint8Array(wasmModule.HEAPU8.buffer, bufferPtr, bufferSize);

    for (let i = 0; i < WIDTH * HEIGHT; i++) {
        const r = buffer[i * 3];
        const g = buffer[i * 3 + 1];
        const b = buffer[i * 3 + 2];
        ledElements[i].style.backgroundColor = `rgb(${r}, ${g}, ${b})`;
        ledElements[i].style.boxShadow = r > 0 || g > 0 || b > 0 ? `0 0 5px rgb(${r}, ${g}, ${b})` : 'none';
    }
}

function handleBinary(data) {
    if (!ledditeCanvas) return;
    if (data.length < 8) return;
    
    const version = data[0];
    const flags = data[1];
    const width = data[2];
    const height = data[3];
    const x_offset = data[4] > 127 ? data[4] - 256 : data[4];
    const y_offset = data[5] > 127 ? data[5] - 256 : data[5];
    const rotation = data[6];
    const brightness = data[7];

    const pixels = data.slice(8);
    const clearBefore = (flags & 0x01) !== 0;
    const isMarquee = (flags & 0x04) !== 0;

    // Allocate memory in WASM heap for the pixels
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
        `${width}x${height} at (${x_offset}, ${y_offset}), Rot: ${rotation * 90}deg ${isMarquee ? '(Marquee)' : ''}`;
}

function animate() {
    if (ledditeCanvas && ledditeCanvas.isMarqueeActive()) {
        ledditeCanvas.updateMarquee(Date.now());
        updateDOM();
    }
    requestAnimationFrame(animate);
}

// WebSocket Connection
function connect() {
    const ws = new WebSocket('ws://localhost:8765');
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => {
        document.getElementById('status-text').textContent = 'Connected (WASM Mode)';
        document.getElementById('status-text').style.color = '#0f0';
    };

    ws.onmessage = (event) => {
        if (event.data instanceof ArrayBuffer) {
            handleBinary(new Uint8Array(event.data));
        }
    };

    ws.onclose = () => {
        document.getElementById('status-text').textContent = 'Disconnected (Reconnecting...)';
        document.getElementById('status-text').style.color = '#f00';
        setTimeout(connect, 2000);
    };
}

document.getElementById('clear-btn').onclick = () => {
    if (ledditeCanvas) {
        ledditeCanvas.stopMarquee();
        ledditeCanvas.clear();
        updateDOM();
    }
};

window.handleLedditePacket = handleBinary;
