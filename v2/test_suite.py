import asyncio
import websockets
import struct
import math
import time

# --- Protocol Helpers ---
def create_packet(width, height, x=0, y=0, rotation=0, flags=0, brightness=255, pixels=None):
    header = struct.pack('BBBBbbBB', 1, flags, width, height, x, y, rotation, brightness)
    if pixels is None:
        pixels = [255, 255, 255] * (width * height)
    return header + bytes(pixels)

async def send_packet(packet):
    async with websockets.connect("ws://localhost:8765") as ws:
        await ws.send(packet)

# --- Test Programs ---

async def draw_shapes():
    print("Test: Drawing Shapes...")
    # 1. Red Square
    red_sq = create_packet(4, 4, x=2, y=2, pixels=[255, 0, 0] * 16, flags=1 | 2)
    await send_packet(red_sq)
    await asyncio.sleep(1)

    # 2. Blue Circle (approximation)
    blue_pixels = []
    for y in range(5):
        for x in range(5):
            dist = math.sqrt((x-2)**2 + (y-2)**2)
            if dist < 2.5: blue_pixels.extend([0, 0, 255])
            else: blue_pixels.extend([0, 0, 0])
    blue_circle = create_packet(5, 5, x=9, y=9, pixels=blue_pixels, flags=0 | 2)
    await send_packet(blue_circle)
    await asyncio.sleep(1)

async def test_rotation():
    print("Test: Rotation...")
    # 2x8 rainbow strip
    rainbow = []
    for i in range(8):
        rainbow.extend([i*32, 255-i*32, 128] * 2)
    
    for rot in range(4):
        p = create_packet(2, 8, x=7, y=4, rotation=rot, pixels=rainbow, flags=1 | 2)
        await send_packet(p)
        await asyncio.sleep(0.5)

async def test_marquee():
    print("Test: Automated Marquee...")
    # A long 16x40 strip of alternating colors
    long_strip = []
    for x in range(40):
        color = [255, 255, 255] if (x // 4) % 2 == 0 else [50, 50, 50]
        long_strip.extend(color * 16)
    
    # Flags: 0x04 (Marquee) | 0x01 (Clear) | 0x02 (Show)
    marquee_packet = create_packet(40, 16, x=0, y=0, pixels=long_strip, flags=1 | 2 | 4)
    await send_packet(marquee_packet)
    await asyncio.sleep(5)

async def test_bouncing_ball():
    print("Test: Bouncing Ball (High FPS Client-side)...")
    x, y = 8, 8
    dx, dy = 1, 1
    async with websockets.connect("ws://localhost:8765") as ws:
        for _ in range(100):
            x += dx
            y += dy
            if x <= 0 or x >= 14: dx *= -1
            if y <= 0 or y >= 14: dy *= -1
            
            ball = create_packet(2, 2, x=int(x), y=int(y), pixels=[255, 255, 0] * 4, flags=1 | 2)
            await ws.send(ball)
            await asyncio.sleep(0.03)

async def run_all():
    print("Starting Leddite V2 Test Suite...")
    try:
        await draw_shapes()
        await test_rotation()
        await test_marquee()
        await test_bouncing_ball()
        print("Test Suite Complete.")
    except Exception as e:
        print(f"Error: {e}. Is the simulator server running (make run-sim)?")

if __name__ == "__main__":
    asyncio.run(run_all())
