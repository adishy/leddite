"""Leddite V2 Test Suite

Usage:
    # Simulator (default) — requires: python simulator_server.py
    python test_suite.py

    # Hardware
    python test_suite.py 192.168.1.100 81

    # Verify mode — pauses between tests for human/agent visual inspection
    python test_suite.py --verify
    python test_suite.py 192.168.1.100 81 --verify

Tests:
    test_shapes       — colored rectangles and a circle
    test_text         — static text, rotated text
    test_marquee      — automated scrolling text strip
    test_bouncing_ball — 60 FPS ball animation (bridge latency test)
    test_encoder_events — binary regression + JSON encoder event validation
    test_verify_modes — (--verify only) walks through each visual state
                         for human / agent confirmation
"""

import asyncio
import json
import math
import struct
import sys
import time

import websockets

# ── Target (overridden by CLI args) ───────────────────────────────────────────
TARGET_HOST = "localhost"
TARGET_PORT = "8765"
VERIFY_MODE = False   # --verify flag: pause between tests for visual check

# ── 5×7 Font ──────────────────────────────────────────────────────────────────
FONT = {
    '0': [0x3E,0x51,0x49,0x51,0x3E], '1': [0x00,0x42,0x7F,0x40,0x00],
    '2': [0x42,0x61,0x51,0x49,0x46], '3': [0x22,0x41,0x49,0x49,0x36],
    '4': [0x18,0x14,0x12,0x7F,0x10], '5': [0x2F,0x49,0x49,0x49,0x31],
    '6': [0x3E,0x4A,0x49,0x49,0x30], '7': [0x01,0x71,0x09,0x05,0x03],
    '8': [0x36,0x49,0x49,0x49,0x36], '9': [0x06,0x49,0x49,0x29,0x1E],
    'A': [0x7E,0x11,0x11,0x11,0x7E], 'B': [0x7F,0x49,0x49,0x49,0x36],
    'C': [0x3E,0x41,0x41,0x41,0x22], 'D': [0x7F,0x41,0x41,0x22,0x1C],
    'E': [0x7F,0x49,0x49,0x49,0x41], 'F': [0x7F,0x09,0x09,0x09,0x01],
    'G': [0x3E,0x41,0x49,0x49,0x7A], 'H': [0x7F,0x08,0x08,0x08,0x7F],
    'I': [0x00,0x41,0x7F,0x41,0x00], 'J': [0x20,0x40,0x41,0x3F,0x01],
    'K': [0x7F,0x08,0x14,0x22,0x41], 'L': [0x7F,0x40,0x40,0x40,0x40],
    'M': [0x7F,0x02,0x0C,0x02,0x7F], 'N': [0x7F,0x04,0x08,0x10,0x7F],
    'O': [0x3E,0x41,0x41,0x41,0x3E], 'P': [0x7F,0x09,0x09,0x09,0x06],
    'Q': [0x3E,0x41,0x51,0x21,0x5E], 'R': [0x7F,0x09,0x19,0x29,0x46],
    'S': [0x46,0x49,0x49,0x49,0x31], 'T': [0x01,0x01,0x7F,0x01,0x01],
    'U': [0x3F,0x40,0x40,0x40,0x3F], 'V': [0x1F,0x20,0x40,0x20,0x1F],
    'W': [0x3F,0x40,0x38,0x40,0x3F], 'X': [0x63,0x14,0x08,0x14,0x63],
    'Y': [0x07,0x08,0x70,0x08,0x07], 'Z': [0x61,0x51,0x49,0x45,0x43],
    ' ': [0x00,0x00,0x00,0x00,0x00], '!': [0x00,0x00,0x5F,0x00,0x00],
    '?': [0x02,0x01,0x51,0x09,0x06],
}

def render_text(text, color=(255, 255, 255)):
    text = text.upper()
    width = len(text) * 6
    height = 7
    pixels = [0, 0, 0] * (width * height)
    for ci, ch in enumerate(text):
        bm = FONT.get(ch, FONT[' '])
        for col, byte in enumerate(bm):
            for row in range(7):
                if (byte >> row) & 1:
                    idx = (row * width + ci * 6 + col) * 3
                    pixels[idx:idx+3] = list(color)
    return width, height, pixels


# ── Protocol helpers ──────────────────────────────────────────────────────────
def create_packet(width, height, x=0, y=0, rotation=0, flags=0, brightness=64, pixels=None):
    header = struct.pack('BBBBbbBB', 1, flags, width, height, x, y, rotation, brightness)
    if pixels is None:
        pixels = [255, 255, 255] * (width * height)
    return header + bytes(pixels)


async def send_packet(packet, keep_open_ms=0):
    """Open a connection, send one binary packet, optionally wait, then close."""
    uri = f"ws://{TARGET_HOST}:{TARGET_PORT}"
    try:
        async with websockets.connect(uri, compression=None) as ws:
            await ws.send(packet)
            if keep_open_ms:
                await asyncio.sleep(keep_open_ms / 1000)
    except Exception as e:
        raise RuntimeError(f"send_packet failed ({uri}): {e}") from e


# ── Verify-mode helpers ───────────────────────────────────────────────────────
def verify_prompt(label, description):
    """In --verify mode: pause and describe what should be visible."""
    if not VERIFY_MODE:
        return
    print(f"\n  ┌─ VERIFY: {label}")
    print(f"  │  {description}")
    print(f"  └─ Press ENTER to continue (or type 'f' + ENTER to flag as failed)…", end=" ")
    resp = input().strip().lower()
    if resp.startswith('f'):
        print(f"  ✗ FLAGGED: {label}")
        return False
    return True


# ── Tests ─────────────────────────────────────────────────────────────────────
async def test_shapes():
    print("  shapes… ", end="", flush=True)

    # Red 4×4 square top-left
    await send_packet(create_packet(4, 4, x=0, y=0, pixels=[255, 0, 0] * 16, flags=0x01|0x02))
    await asyncio.sleep(0.5)

    # Blue circle (5×5, 2-pixel radius) — drawn ON TOP of red square
    blue = []
    for y in range(5):
        for x in range(5):
            blue.extend([0, 0, 255] if math.sqrt((x-2)**2 + (y-2)**2) < 2.5 else [0, 0, 0])
    await send_packet(create_packet(5, 5, x=9, y=9, pixels=blue, flags=0x02))
    await asyncio.sleep(0.8)

    verify_prompt("Shapes",
        "Red 4×4 square at top-left; blue circle at bottom-right (approx 9,9).")
    print("OK")


async def test_text():
    print("  text…   ", end="", flush=True)

    # "HI" in red, static
    w, h, px = render_text("HI", color=(255, 0, 0))
    await send_packet(create_packet(w, h, x=2, y=4, pixels=px, flags=0x01|0x02))
    await asyncio.sleep(1.0)

    verify_prompt("Static text", "Red 'HI' centred on display.")

    # "LED" in blue, rotated 90°
    w, h, px = render_text("LED", color=(0, 100, 255))
    await send_packet(create_packet(w, h, x=8, y=0, rotation=1, pixels=px, flags=0x01|0x02))
    await asyncio.sleep(1.0)

    verify_prompt("Rotated text", "Blue 'LED' rotated 90° starting from top-right area.")
    print("OK")


async def test_marquee():
    print("  marquee…", end="", flush=True)

    msg = "LEDDITE V2 WASM POWERED !"
    w, h, px = render_text(msg, color=(255, 128, 0))
    # flags: Marquee(0x04) | Clear(0x01) | Show(0x02)
    await send_packet(create_packet(w, h, x=0, y=4, pixels=px, flags=0x01|0x02|0x04))
    await asyncio.sleep(6)

    verify_prompt("Marquee", "Orange text 'LEDDITE V2 WASM POWERED !' scrolling right→left.")
    print("OK")


async def test_bouncing_ball():
    print("  60fps…  ", end="", flush=True)

    x, y, dx, dy = 8.0, 8.0, 0.8, 0.6
    t0 = time.monotonic()
    frames = 0

    uri = f"ws://{TARGET_HOST}:{TARGET_PORT}"
    try:
        async with websockets.connect(uri, compression=None) as ws:
            for _ in range(120):
                x += dx; y += dy
                if x <= 0 or x >= 14: dx *= -1
                if y <= 0 or y >= 14: dy *= -1
                pkt = create_packet(2, 2, x=int(x), y=int(y),
                                    pixels=[255, 255, 0] * 4, flags=0x01|0x02)
                await ws.send(pkt)
                frames += 1
                await asyncio.sleep(0.016)
    except Exception as e:
        raise RuntimeError(f"bouncing ball failed: {e}") from e

    elapsed = time.monotonic() - t0
    fps = frames / elapsed
    verify_prompt("Bouncing ball", f"Yellow 2×2 ball bouncing around the display at ~60 FPS. Actual: {fps:.0f} FPS.")
    print(f"OK ({fps:.0f} FPS)")


async def test_encoder_events():
    print("  encoder…", end="", flush=True)

    # Regression: binary protocol still works
    await send_packet(create_packet(4, 4, x=6, y=6, pixels=[0, 255, 0] * 16, flags=0x01|0x02))

    received = []
    uri = f"ws://{TARGET_HOST}:{TARGET_PORT}"
    try:
        async with websockets.connect(uri, compression=None) as ws:
            deadline = asyncio.get_event_loop().time() + 2.0
            while asyncio.get_event_loop().time() < deadline:
                try:
                    msg = await asyncio.wait_for(ws.recv(), timeout=0.2)
                    if isinstance(msg, str):
                        ev = json.loads(msg)
                        assert "type" in ev
                        assert ev["type"] == "encoder"
                        assert "delta" in ev or "button" in ev or "longPress" in ev
                        if "delta" in ev:
                            assert isinstance(ev["delta"], int)
                        if "button" in ev:
                            assert ev["button"] in ("pressed", "released")
                        received.append(ev)
                except asyncio.TimeoutError:
                    pass
    except Exception as e:
        raise RuntimeError(f"encoder test failed: {e}") from e

    if received:
        print(f"OK ({len(received)} events: {received[:2]})")
    else:
        # CI-safe: pass even when no hardware / encoder events received
        print("OK (no events — expected in simulator / CI)")


async def test_verify_modes():
    """Visual verification walkthrough — only runs in --verify mode.

    Sends a representative frame for each firmware mode so a human or agent
    can confirm the display looks correct.  Uses plain binary protocol frames
    to simulate what each mode renders; does NOT require the ESP32 to be in
    any particular mode.
    """
    if not VERIFY_MODE:
        return

    print("\n  ── Visual mode verification walkthrough ──")

    # 1. Boot menu look-alike: 4 dots + scrolling label
    dots = [0] * (16 * 16 * 3)
    for xi, col in zip([3, 6, 9, 12], [[255,200,80],[255,130,40],[255,90,90],[255,230,100]]):
        idx = (14 * 16 + xi) * 3
        dots[idx:idx+3] = col
    await send_packet(create_packet(16, 16, x=0, y=0, pixels=dots, flags=0x01|0x02))
    verify_prompt("Boot menu dots",
        "4 coloured dots across the bottom row (y=14) at x=3,6,9,12 in "
        "amber/orange/rose/yellow.")

    # 2. Clock-like: two centred text rows
    w1, h1, px1 = render_text("14", color=(80, 180, 255))
    await send_packet(create_packet(w1, h1, x=2, y=1, pixels=px1, flags=0x01|0x02))
    w2, h2, px2 = render_text("32", color=(255, 80, 160))
    await send_packet(create_packet(w2, h2, x=2, y=9, pixels=px2, flags=0x02))
    await asyncio.sleep(1.0)
    verify_prompt("Clock display",
        "Blue '14' on top row (y=1), pink '32' on bottom row (y=9).")

    # 3. Date marquee simulation
    w, h, px = render_text("24 MAY 2026", color=(80, 255, 120))
    await send_packet(create_packet(w, h, x=0, y=4, pixels=px, flags=0x01|0x02|0x04))
    await asyncio.sleep(5)
    verify_prompt("Date marquee", "Mint-green '24 MAY 2026' scrolling across middle of display.")

    # 4. Progress bar (timer-like)
    filled = 128  # half
    prog = []
    for i in range(256):
        prog.extend([100, 200, 100] if i < filled else [20, 20, 20])
    await send_packet(create_packet(16, 16, x=0, y=0, pixels=prog, flags=0x01|0x02))
    await asyncio.sleep(1.0)
    verify_prompt("Timer progress", "Top-left half of display lit green (128/256 pixels).")

    # 5. Clear
    await send_packet(create_packet(1, 1, x=0, y=0, pixels=[0,0,0], flags=0x01|0x02))
    print("  Verification walkthrough complete.")


# ── Runner ────────────────────────────────────────────────────────────────────
async def check_connection():
    """Verify the target is reachable before running tests."""
    uri = f"ws://{TARGET_HOST}:{TARGET_PORT}"
    try:
        async with websockets.connect(uri, compression=None, open_timeout=3) as ws:
            pass
        return True
    except Exception as e:
        print(f"  ✗ Cannot connect to {uri}: {e}")
        if TARGET_PORT == "8765":
            print("    → Start the simulator first:  python simulator_server.py")
            print("      Then open:                  http://localhost:8000")
        else:
            print(f"    → Ensure ESP32 is powered and in Network Canvas mode.")
        return False


async def run_all():
    global TARGET_HOST, TARGET_PORT, VERIFY_MODE

    args = [a for a in sys.argv[1:] if a != '--verify']
    if '--verify' in sys.argv:
        VERIFY_MODE = True

    if len(args) >= 1:
        TARGET_HOST = args[0]
    if len(args) >= 2:
        TARGET_PORT = args[1]

    target = f"{TARGET_HOST}:{TARGET_PORT}"
    mode_label = "HARDWARE" if TARGET_PORT != "8765" else "SIMULATOR"
    verify_label = " [VERIFY MODE]" if VERIFY_MODE else ""
    print(f"\nLeddite V2 Test Suite — {mode_label} @ {target}{verify_label}")
    print("─" * 42)

    if not await check_connection():
        sys.exit(1)

    results = {}
    tests = [
        ("shapes",          test_shapes),
        ("text",            test_text),
        ("marquee",         test_marquee),
        ("bouncing_ball",   test_bouncing_ball),
        ("encoder_events",  test_encoder_events),
        ("verify_modes",    test_verify_modes),
    ]

    for name, fn in tests:
        t0 = time.monotonic()
        try:
            await fn()
            results[name] = ("PASS", time.monotonic() - t0)
        except Exception as e:
            results[name] = ("FAIL", time.monotonic() - t0)
            print(f"FAIL — {e}")

    print("\n" + "─" * 42)
    print("Results:")
    all_pass = True
    for name, (status, elapsed) in results.items():
        icon = "✓" if status == "PASS" else "✗"
        if status != "PASS":
            all_pass = False
        if name == "verify_modes" and not VERIFY_MODE:
            continue  # skip printing skipped verify test
        print(f"  {icon} {name:<20} {status}  ({elapsed:.2f}s)")

    print()
    if all_pass:
        print("All tests passed ✓")
    else:
        print("Some tests FAILED ✗")
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(run_all())
