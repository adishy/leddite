const WIDTH = 16;
const HEIGHT = 16;
const ledElements = [];
const buffer = Array(WIDTH * HEIGHT).fill({r: 0, g: 0, b: 0});

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
    for (let i = 0; i < WIDTH * HEIGHT; i++) {
        const {r, g, b} = buffer[i];
        ledElements[i].style.backgroundColor = `rgb(${r}, ${g}, ${b})`;
        ledElements[i].style.boxShadow = r > 0 || g > 0 || b > 0 ? `0 0 5px rgb(${r}, ${g}, ${b})` : 'none';
    }
}

function clear() {
    for (let i = 0; i < WIDTH * HEIGHT; i++) {
        buffer[i] = {r: 0, g: 0, b: 0};
    }
}

function transformCoords(sx, sy, w, h, rotation) {
    let tx, ty;
    switch (rotation) {
        case 1: // 90 deg
            tx = h - 1 - sy;
            ty = sx;
            break;
        case 2: // 180 deg
            tx = w - 1 - sx;
            ty = h - 1 - sy;
            break;
        case 3: // 270 deg
            tx = sy;
            ty = w - 1 - sx;
            break;
        case 0:
        default:
            tx = sx;
            ty = sy;
            break;
    }
    return {tx, ty};
}

function drawSprite(data, w, h, x_offset, y_offset, rotation, clearBefore) {
    if (clearBefore) clear();

    for (let sy = 0; sy < h; sy++) {
        for (let sx = 0; sx < w; sx++) {
            const {tx, ty} = transformCoords(sx, sy, w, h, rotation);
            const px = tx + x_offset;
            const py = ty + y_offset;

            if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT) continue;

            const spriteIdx = (sy * w + sx) * 3;
            const canvasIdx = py * WIDTH + px;

            buffer[canvasIdx] = {
                r: data[spriteIdx],
                g: data[spriteIdx + 1],
                b: data[spriteIdx + 2]
            };
        }
    }
    updateDOM();
}

function handleBinary(data) {
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

    drawSprite(pixels, width, height, x_offset, y_offset, rotation, clearBefore);
    
    document.getElementById('sprite-info').textContent = 
        `${width}x${height} at (${x_offset}, ${y_offset}), Rot: ${rotation * 90}deg`;
}

// In a real scenario, this would be a WebSocket.
// For testing, we'll expose it to the window.
window.handleLedditePacket = handleBinary;

document.getElementById('clear-btn').onclick = () => {
    clear();
    updateDOM();
};

initGrid();
updateDOM();
console.log("Leddite V2 Simulator Ready.");
console.log("Usage: window.handleLedditePacket(new Uint8Array([...]))");
