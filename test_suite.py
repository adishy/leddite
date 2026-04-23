import asyncio
import websockets
import struct
import math
import time

# --- 5x7 Font Data (0-9, A-Z, Space, !, ?) ---
FONT = {
    '0': [0x3E, 0x51, 0x49, 0x51, 0x3E], '1': [0x00, 0x42, 0x7F, 0x40, 0x00],
    '2': [0x42, 0x61, 0x51, 0x49, 0x46], '3': [0x22, 0x41, 0x49, 0x49, 0x36],
    '4': [0x18, 0x14, 0x12, 0x7F, 0x10], '5': [0x2F, 0x49, 0x49, 0x49, 0x31],
    '6': [0x3E, 0x4A, 0x49, 0x49, 0x30], '7': [0x01, 0x71, 0x09, 0x05, 0x03],
    '8': [0x36, 0x49, 0x49, 0x49, 0x36], '9': [0x06, 0x49, 0x49, 0x29, 0x1E],
    'A': [0x7E, 0x11, 0x11, 0x11, 0x7E], 'B': [0x7F, 0x49, 0x49, 0x49, 0x36],
    'C': [0x3E, 0x41, 0x41, 0x41, 0x22], 'D': [0x7F, 0x41, 0x41, 0x22, 0x1C],
    'E': [0x7F, 0x49, 0x49, 0x49, 0x41], 'F': [0x7F, 0x09, 0x09, 0x09, 0x01],
    'G': [0x3E, 0x41, 0x49, 0x49, 0x7A], 'H': [0x7F, 0x08, 0x08, 0x08, 0x7F],
    'I': [0x00, 0x41, 0x7F, 0x41, 0x00], 'J': [0x20, 0x40, 0x41, 0x3F, 0x01],
    'K': [0x7F, 0x08, 0x14, 0x22, 0x41], 'L': [0x7F, 0x40, 0x40, 0x40, 0x40],
    'M': [0x7F, 0x02, 0x0C, 0x02, 0x7F], 'N': [0x7F, 0x04, 0x08, 0x10, 0x7F],
    'O': [0x3E, 0x41, 0x41, 0x41, 0x3E], 'P': [0x7F, 0x09, 0x09, 0x09, 0x06],
    'Q': [0x3E, 0x41, 0x51, 0x21, 0x5E], 'R': [0x7F, 0x09, 0x19, 0x29, 0x46],
    'S': [0x46, 0x49, 0x49, 0x49, 0x31], 'T': [0x01, 0x01, 0x7F, 0x01, 0x01],
    'U': [0x3F, 0x40, 0x40, 0x40, 0x3F], 'V': [0x1F, 0x20, 0x40, 0x20, 0x1F],
    'W': [0x3F, 0x40, 0x38, 0x40, 0x3F], 'X': [0x63, 0x14, 0x08, 0x14, 0x63],
    'Y': [0x07, 0x08, 0x70, 0x08, 0x07], 'Z': [0x61, 0x51, 0x49, 0x45, 0x43],
    ' ': [0x00, 0x00, 0x00, 0x00, 0x00], '!': [0x00, 0x00, 0x5F, 0x00, 0x00],
    '?': [0x02, 0x01, 0x51, 0x09, 0x06],
}

def render_text(text, color=(255, 255, 255)):
    text = text.upper()
    width = len(text) * 6
    height = 7
    pixels = [0, 0, 0] * (width * height)
    for char_idx, char in enumerate(text):
        bitmap = FONT.get(char, FONT[' '])
        for col_idx, col_byte in enumerate(bitmap):
            for row_idx in range(7):
                if (col_byte >> row_idx) & 1:
                    x, y = char_idx * 6 + col_idx, row_idx
                    idx = (y * width + x) * 3
                    pixels[idx:idx+3] = color
    return width, height, pixels

# --- Protocol Helpers ---
def create_packet(width, height, x=0, y=0, rotation=0, flags=0, brightness=255, pixels=None):
    header = struct.pack('BBBBbbBB', 1, flags, width, height, x, y, rotation, brightness)
    if pixels is None: pixels = [255, 255, 255] * (width * height)
    return header + bytes(pixels)

async def send_packet(packet):
    async with websockets.connect("ws://localhost:8765") as ws:
        await ws.send(packet)

# --- Test Programs ---

async def test_shapes():
    print("Test: Shapes...")
    await send_packet(create_packet(4, 4, x=2, y=2, pixels=[255, 0, 0] * 16, flags=1|2))
    await asyncio.sleep(1)
    
    blue_pixels = []
    for y in range(5):
        for x in range(5):
            dist = math.sqrt((x-2)**2 + (y-2)**2)
            blue_pixels.extend([0, 0, 255] if dist < 2.5 else [0, 0, 0])
    await send_packet(create_packet(5, 5, x=9, y=9, pixels=blue_pixels, flags=2))
    await asyncio.sleep(1)

async def test_text():
    print("Test: Text (Static & Rotated)...")
    # Static HI
    w, h, pixels = render_text("HI", color=(0, 255, 0))
    await send_packet(create_packet(w, h, x=2, y=4, pixels=pixels, flags=1|2))
    await asyncio.sleep(1.5)

    # Rotated LED
    w, h, pixels = render_text("LED", color=(0, 100, 255))
    await send_packet(create_packet(w, h, x=8, y=0, rotation=1, pixels=pixels, flags=1|2))
    await asyncio.sleep(1.5)

async def test_marquee():
    print("Test: Automated Marquee (Text Strip)...")
    msg = "LEDDITE V2 WASM POWERED !"
    w, h, pixels = render_text(msg, color=(255, 128, 0))
    # Flags: 0x04 (Marquee) | 0x01 (Clear) | 0x02 (Show)
    await send_packet(create_packet(w, h, x=0, y=4, pixels=pixels, flags=1|2|4))
    await asyncio.sleep(8)

async def test_bouncing_ball():
    print("Test: Bouncing Ball (60 FPS Bridge Test)...")
    x, y, dx, dy = 8, 8, 0.8, 0.6
    async with websockets.connect("ws://localhost:8765") as ws:
        for _ in range(120):
            x, y = x + dx, y + dy
            if x <= 0 or x >= 14: dx *= -1
            if y <= 0 or y >= 14: dy *= -1
            await ws.send(create_packet(2, 2, x=int(x), y=int(y), pixels=[255, 255, 0] * 4, flags=1|2))
            await asyncio.sleep(0.016) # 60 FPS

async def run_all():
    print("Starting Comprehensive Leddite V2 Suite...")
    try:
        await test_shapes()
        await test_text()
        await test_marquee()
        await test_bouncing_ball()
        print("Suite Complete.")
    except Exception as e:
        print(f"Error: {e}. Is 'make run-sim' active?")

if __name__ == "__main__":
    asyncio.run(run_all())
