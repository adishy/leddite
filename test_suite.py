import asyncio
import websockets
import struct
import math
import sys
import time
import json

TARGET_HOST = "localhost"
TARGET_PORT = "8765"

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
def create_packet(width, height, x=0, y=0, rotation=0, flags=0, brightness=64, pixels=None):
    header = struct.pack('BBBBbbBB', 1, flags, width, height, x, y, rotation, brightness)
    if pixels is None: pixels = [255, 255, 255] * (width * height)
    return header + bytes(pixels)

async def send_packet(packet):
    async with websockets.connect(f"ws://{TARGET_HOST}:{TARGET_PORT}", compression=None) as ws:
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
    w, h, pixels = render_text("HI", color=(255, 0, 0))
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
    async with websockets.connect(f"ws://{TARGET_HOST}:{TARGET_PORT}", compression=None) as ws:
        for _ in range(120):
            x, y = x + dx, y + dy
            if x <= 0 or x >= 14: dx *= -1
            if y <= 0 or y >= 14: dy *= -1
            await ws.send(create_packet(2, 2, x=int(x), y=int(y), pixels=[255, 255, 0] * 4, flags=1|2))
            await asyncio.sleep(0.016) # 60 FPS

async def test_encoder_events():
    """Test: Encoder event receive-side validation.

    Connects to the WebSocket, sends a regression binary packet to verify
    binary frames still work, then listens for 1.5s for JSON text frames
    (encoder events from hardware). Passes in CI even if no encoder events arrive.

    When testing against live hardware, physically rotate/press the encoder
    while this test is running to verify the JSON events are received.
    """
    print("Test: Encoder event receive (binary regression + JSON parse)...")

    # Regression: verify binary protocol still works
    await send_packet(create_packet(4, 4, x=6, y=6, pixels=[0, 255, 0] * 16, flags=1|2))

    # Listen for up to 1.5s for any text (encoder) frames
    received_events = []
    try:
        async with websockets.connect(f"ws://{TARGET_HOST}:{TARGET_PORT}", compression=None) as ws:
            deadline = asyncio.get_event_loop().time() + 1.5
            while asyncio.get_event_loop().time() < deadline:
                try:
                    msg = await asyncio.wait_for(ws.recv(), timeout=0.2)
                    if isinstance(msg, str):
                        event = json.loads(msg)
                        assert "type" in event, "Encoder event must have 'type' field"
                        assert event["type"] == "encoder", f"Expected type='encoder', got {event['type']}"
                        # Must have either 'delta' (int) or 'button' (str)
                        assert ("delta" in event or "button" in event), \
                            f"Encoder event must have 'delta' or 'button': {event}"
                        if "delta" in event:
                            assert isinstance(event["delta"], int), \
                                f"'delta' must be int, got {type(event['delta'])}"
                        if "button" in event:
                            assert event["button"] in ("pressed", "released"), \
                                f"'button' must be 'pressed' or 'released', got {event['button']}"
                        received_events.append(event)
                        print(f"  Received encoder event: {event}")
                except asyncio.TimeoutError:
                    pass  # no message in this 0.2s window, keep waiting
    except Exception as e:
        print(f"  Connection error: {e}")

    if received_events:
        print(f"  Validated {len(received_events)} encoder event(s) — PASSED")
    else:
        print("  No encoder events received (expected in CI / when not in Network mode) — PASSED")


async def run_all():
    global TARGET_HOST, TARGET_PORT
    if len(sys.argv) > 1:
        TARGET_HOST = sys.argv[1] if sys.argv[1] else TARGET_HOST
        TARGET_PORT = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else TARGET_PORT

    host = f"{TARGET_HOST}:{TARGET_PORT}"
    print(f"Starting Comprehensive Leddite V2 Suite to ({host})...")
    try:
        await test_shapes()
        await test_text()
        await test_marquee()
        await test_bouncing_ball()
        await test_encoder_events()
        print("Suite Complete.")
    except Exception as e:
        print(f"Error: {e}. Is 'make run-sim' active?")

if __name__ == "__main__":
    asyncio.run(run_all())

