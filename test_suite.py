"""Leddite V2 Test Suite

Usage:
    # Simulator (default) — requires: python simulator_server.py
    python test_suite.py

    # Hardware (binary protocol only; canvas assertions skipped)
    python test_suite.py 192.168.1.100 81

    # Verify mode — pauses for human / agent visual inspection
    python test_suite.py --verify
    python test_suite.py 192.168.1.100 81 --verify

Canvas assertions (simulator only):
    When targeting the simulator server at localhost:8765, every test
    fetches the rendered canvas state from http://localhost:8000/canvas-state
    and makes pixel-level assertions.  This gives agents a deterministic,
    vision-free way to confirm that packets were rendered correctly.

    Hardware targets skip canvas assertions (no render feedback channel).
"""

import asyncio
import json
import math
import struct
import sys
import time
import urllib.request

import websockets

# ── CLI config ────────────────────────────────────────────────────────────────
TARGET_HOST  = "localhost"
TARGET_PORT  = "8765"
VERIFY_MODE  = False   # --verify: pause between tests for visual inspection
CANVAS_API   = "http://localhost:8000/canvas-state"  # simulator only

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
    uri = f"ws://{TARGET_HOST}:{TARGET_PORT}"
    try:
        async with websockets.connect(uri, compression=None) as ws:
            await ws.send(packet)
            if keep_open_ms:
                await asyncio.sleep(keep_open_ms / 1000)
    except Exception as e:
        raise RuntimeError(f"send_packet failed ({uri}): {e}") from e


# ── Canvas assertion helpers ──────────────────────────────────────────────────

def _using_simulator():
    return TARGET_HOST == "localhost" and TARGET_PORT == "8765"


def get_canvas_state():
    """Fetch the current 16×16 canvas from the simulator server.

    Returns a dict with keys:
      width, height  — always 16
      pixels         — list of 256 [r, g, b] entries, row-major (y=0 first)

    Only available when targeting the simulator.  Returns None for hardware targets.
    """
    if not _using_simulator():
        return None
    try:
        with urllib.request.urlopen(CANVAS_API, timeout=2) as resp:
            return json.loads(resp.read())
    except Exception as e:
        raise RuntimeError(f"Could not fetch canvas state from {CANVAS_API}: {e}\n"
                           "  Is simulator_server.py running?") from e


def get_pixel(state, x, y):
    """Return [r, g, b] for the pixel at (x, y) from a canvas state dict."""
    return state["pixels"][y * 16 + x]


def colors_close(a, b, tolerance=30):
    """True if two [r,g,b] colors are within tolerance on each channel."""
    return all(abs(a[i] - b[i]) <= tolerance for i in range(3))


def assert_pixel(state, x, y, expected, tolerance=30, label=""):
    """Assert pixel (x,y) is approximately expected [r,g,b]."""
    actual = get_pixel(state, x, y)
    if not colors_close(actual, expected, tolerance):
        tag = f" ({label})" if label else ""
        raise AssertionError(
            f"Pixel ({x},{y}){tag}: expected ~{expected}, got {actual}"
        )


def assert_pixel_nonblack(state, x, y, label=""):
    """Assert pixel (x,y) is not black (i.e. something was drawn there)."""
    actual = get_pixel(state, x, y)
    if actual == [0, 0, 0]:
        tag = f" ({label})" if label else ""
        raise AssertionError(f"Pixel ({x},{y}){tag}: expected non-black, got [0,0,0]")


def assert_pixel_black(state, x, y, label=""):
    """Assert pixel (x,y) is black (nothing drawn)."""
    actual = get_pixel(state, x, y)
    if actual != [0, 0, 0]:
        tag = f" ({label})" if label else ""
        raise AssertionError(f"Pixel ({x},{y}){tag}: expected [0,0,0] (black), got {actual}")


def assert_region(state, x, y, w, h, expected_color, min_fraction=0.7, tolerance=30, label=""):
    """Assert that at least min_fraction of a region matches expected_color."""
    total = w * h
    hits = sum(
        1 for py in range(y, y + h) for px in range(x, x + w)
        if colors_close(get_pixel(state, px, py), expected_color, tolerance)
    )
    fraction = hits / total
    if fraction < min_fraction:
        tag = f" ({label})" if label else ""
        raise AssertionError(
            f"Region ({x},{y} {w}×{h}){tag}: only {fraction:.0%} of pixels "
            f"match ~{expected_color} (need ≥{min_fraction:.0%})"
        )


def assert_any_nonblack(state, label=""):
    """Assert the canvas has at least one non-black pixel."""
    for px in state["pixels"]:
        if px != [0, 0, 0]:
            return
    tag = f" ({label})" if label else ""
    raise AssertionError(f"Canvas is completely black{tag} — nothing was rendered")


# ── Verify-mode helpers ───────────────────────────────────────────────────────
def verify_prompt(label, description):
    if not VERIFY_MODE:
        return True
    print(f"\n  ┌─ VERIFY: {label}")
    print(f"  │  {description}")
    print(f"  └─ ENTER to continue, 'f'+ENTER to flag…", end=" ")
    resp = input().strip().lower()
    if resp.startswith('f'):
        print(f"  ✗ FLAGGED: {label}")
        return False
    return True


# ── Tests ─────────────────────────────────────────────────────────────────────

async def test_shapes():
    print("  shapes… ", end="", flush=True)

    # Red 4×4 square at (2,2), clear first
    await send_packet(create_packet(4, 4, x=2, y=2, pixels=[255, 0, 0] * 16, flags=0x01|0x02))
    await asyncio.sleep(0.3)

    state = get_canvas_state()
    if state:
        assert_region(state, 2, 2, 4, 4, [255, 0, 0], min_fraction=0.9, label="red square")
        assert_pixel_black(state, 0, 0, label="outside square")

    # Blue circle (5×5) at (9,9), drawn on top
    blue = []
    for sy in range(5):
        for sx in range(5):
            blue.extend([0, 0, 255] if math.sqrt((sx-2)**2 + (sy-2)**2) < 2.5 else [0, 0, 0])
    await send_packet(create_packet(5, 5, x=9, y=9, pixels=blue, flags=0x02))
    await asyncio.sleep(0.3)

    state = get_canvas_state()
    if state:
        assert_pixel(state, 11, 11, [0, 0, 255], label="blue circle centre")
        assert_pixel(state, 3,  3,  [255, 0, 0], label="red square still present")

    verify_prompt("Shapes", "Red 4×4 square at top-left area; blue circle at bottom-right.")
    print("OK")


async def test_text():
    print("  text…   ", end="", flush=True)

    # "HI" in red, clear first
    w, h, px = render_text("HI", color=(255, 0, 0))
    await send_packet(create_packet(w, h, x=2, y=4, pixels=px, flags=0x01|0x02))
    await asyncio.sleep(0.3)

    state = get_canvas_state()
    if state:
        assert_any_nonblack(state, label="HI text")
        # The 'H' first column glyph (0x7F) — column 0 of 'H' is all 7 rows lit.
        # At x=2 (sprite x=0 → canvas x=2), rows 0..6 → canvas y=4..10
        assert_pixel_nonblack(state, 2, 4, label="H top-left pixel")

    verify_prompt("Static text", "Red 'HI' text at approx (2,4).")

    # "LED" in blue, rotated 90°
    w, h, px = render_text("LED", color=(0, 100, 255))
    await send_packet(create_packet(w, h, x=8, y=0, rotation=1, pixels=px, flags=0x01|0x02))
    await asyncio.sleep(0.3)

    state = get_canvas_state()
    if state:
        assert_any_nonblack(state, label="LED rotated text")

    verify_prompt("Rotated text", "Blue 'LED' rotated 90° from top-right area.")
    print("OK")


async def test_marquee():
    print("  marquee…", end="", flush=True)

    msg = "LEDDITE V2 WASM POWERED !"
    w, h, px = render_text(msg, color=(255, 128, 0))
    await send_packet(create_packet(w, h, x=0, y=4, pixels=px, flags=0x01|0x02|0x04))
    # Marquee starts at x=16 (off-screen right) and scrolls left — no canvas
    # assertion here because render position is time-dependent.
    await asyncio.sleep(6)  # let it scroll a full pass
    verify_prompt("Marquee", "Orange 'LEDDITE V2 WASM POWERED !' scrolling right→left.")
    print("OK")


async def test_bouncing_ball():
    print("  60fps…  ", end="", flush=True)

    x, y, dx, dy = 8.0, 8.0, 0.8, 0.6
    t0 = time.monotonic()
    frames = 0
    last_x = last_y = 0

    uri = f"ws://{TARGET_HOST}:{TARGET_PORT}"
    try:
        async with websockets.connect(uri, compression=None) as ws:
            for _ in range(120):
                x += dx; y += dy
                if x <= 0 or x >= 14: dx *= -1
                if y <= 0 or y >= 14: dy *= -1
                ix, iy = int(x), int(y)
                last_x, last_y = ix, iy
                pkt = create_packet(2, 2, x=ix, y=iy,
                                    pixels=[255, 255, 0] * 4, flags=0x01|0x02)
                await ws.send(pkt)
                frames += 1
                await asyncio.sleep(0.016)
    except Exception as e:
        raise RuntimeError(f"bouncing ball failed: {e}") from e

    elapsed = time.monotonic() - t0
    fps = frames / elapsed

    state = get_canvas_state()
    if state:
        # After animation the last frame should be yellow at (last_x, last_y)
        assert_pixel(state, last_x, last_y, [255, 255, 0], label="ball final position")

    verify_prompt("Bouncing ball",
        f"Yellow 2×2 ball bouncing around the display at ~60 FPS. Actual: {fps:.0f} FPS.")
    print(f"OK ({fps:.0f} FPS)")


async def test_encoder_events():
    print("  encoder…", end="", flush=True)

    # Regression: binary protocol still works — draw green square
    await send_packet(create_packet(4, 4, x=6, y=6, pixels=[0, 255, 0] * 16, flags=0x01|0x02))
    await asyncio.sleep(0.2)

    state = get_canvas_state()
    if state:
        assert_region(state, 6, 6, 4, 4, [0, 255, 0], label="encoder regression green square")

    # Listen 2s for JSON encoder events (hardware) or simulator button presses
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
        print("OK (no events — expected in simulator / CI)")


async def test_canvas_assertions():
    """Standalone canvas assertion smoke test — simulator only.

    Sends a known exact pattern and verifies every asserted pixel matches.
    This is the primary test for agents: deterministic pass/fail with no
    browser or vision required.
    """
    if not _using_simulator():
        print("  canvas…  SKIP (hardware target — no canvas feedback)")
        return

    print("  canvas…  ", end="", flush=True)

    # 1. Clear
    await send_packet(create_packet(1, 1, x=0, y=0, pixels=[0, 0, 0], flags=0x01|0x02))
    await asyncio.sleep(0.1)
    state = get_canvas_state()
    for y in range(16):
        for x in range(16):
            assert_pixel_black(state, x, y, label="after clear")

    # 2. Full 16×16 white fill
    await send_packet(create_packet(16, 16, x=0, y=0,
                                    pixels=[255, 255, 255] * 256, flags=0x01|0x02))
    await asyncio.sleep(0.1)
    state = get_canvas_state()
    for y in range(16):
        for x in range(16):
            assert_pixel(state, x, y, [255, 255, 255], label="white fill")

    # 3. Overlay a 2×2 red dot at (7,7) without clearing
    await send_packet(create_packet(2, 2, x=7, y=7, pixels=[255, 0, 0] * 4, flags=0x02))
    await asyncio.sleep(0.1)
    state = get_canvas_state()
    assert_pixel(state, 7,  7,  [255, 0, 0],     label="red dot")
    assert_pixel(state, 8,  7,  [255, 0, 0],     label="red dot")
    assert_pixel(state, 0,  0,  [255, 255, 255],  label="white background preserved")
    assert_pixel(state, 15, 15, [255, 255, 255],  label="white background preserved corner")

    # 4. 90° rotation: 3×1 horizontal bar → should appear as 1×3 vertical bar
    # Sprite: 3 wide × 1 tall, all blue, placed at (4,4), rotated 90°
    # After 90° rotation: w=1, h=3; tx=(h-1-y)=0, ty=x → pixels at canvas (4,4),(4,5),(4,6)
    await send_packet(create_packet(3, 1, x=4, y=4, rotation=1,
                                    pixels=[0, 0, 255] * 3, flags=0x01|0x02))
    await asyncio.sleep(0.1)
    state = get_canvas_state()
    assert_pixel(state, 4, 4, [0, 0, 255], label="rotation 90° pixel 0")
    assert_pixel(state, 4, 5, [0, 0, 255], label="rotation 90° pixel 1")
    assert_pixel(state, 4, 6, [0, 0, 255], label="rotation 90° pixel 2")
    assert_pixel_black(state, 5, 4, label="rotation 90° no overflow")

    # 5. Negative x_offset clipping: 4×4 green at (-2, 0) — left 2 cols off-screen
    await send_packet(create_packet(4, 4, x=-2, y=0, pixels=[0, 255, 0] * 16, flags=0x01|0x02))
    await asyncio.sleep(0.1)
    state = get_canvas_state()
    assert_pixel(state, 0, 0, [0, 255, 0], label="clip left — col 0 visible")
    assert_pixel(state, 1, 0, [0, 255, 0], label="clip left — col 1 visible")
    assert_pixel_black(state, 4, 0, label="clip left — beyond sprite right edge")

    # Clean up
    await send_packet(create_packet(1, 1, x=0, y=0, pixels=[0, 0, 0], flags=0x01|0x02))

    print("OK (5 sub-tests: clear, fill, overlay, rotation, clipping)")


async def test_verify_modes():
    """Visual walkthrough — only runs in --verify mode."""
    if not VERIFY_MODE:
        return

    print("\n  ── Visual mode verification walkthrough ──")

    # Boot menu dots
    dots = [0] * (16 * 16 * 3)
    for xi, col in zip([3,6,9,12], [[255,200,80],[255,130,40],[255,90,90],[255,230,100]]):
        idx = (14 * 16 + xi) * 3
        dots[idx:idx+3] = col
    await send_packet(create_packet(16, 16, x=0, y=0, pixels=dots, flags=0x01|0x02))
    verify_prompt("Boot menu dots",
        "4 warm-colored dots at y=14, x=3/6/9/12 (amber, orange, rose, yellow).")

    # Clock-like display
    w1, h1, px1 = render_text("14", color=(80, 180, 255))
    await send_packet(create_packet(w1, h1, x=2, y=1, pixels=px1, flags=0x01|0x02))
    w2, h2, px2 = render_text("32", color=(255, 80, 160))
    await send_packet(create_packet(w2, h2, x=2, y=9, pixels=px2, flags=0x02))
    await asyncio.sleep(0.8)
    verify_prompt("Clock display", "Blue '14' top row (y=1), pink '32' bottom row (y=9).")

    # Date marquee
    w, h, px = render_text("24 MAY 2026", color=(80, 255, 120))
    await send_packet(create_packet(w, h, x=0, y=4, pixels=px, flags=0x01|0x02|0x04))
    await asyncio.sleep(5)
    verify_prompt("Date marquee", "Mint-green '24 MAY 2026' scrolling across middle of display.")

    # Timer progress bar
    prog = []
    for i in range(256):
        prog.extend([100, 200, 100] if i < 128 else [20, 20, 20])
    await send_packet(create_packet(16, 16, x=0, y=0, pixels=prog, flags=0x01|0x02))
    await asyncio.sleep(0.8)
    verify_prompt("Timer progress", "Top-left half of display lit green (128/256 pixels).")

    await send_packet(create_packet(1, 1, x=0, y=0, pixels=[0,0,0], flags=0x01|0x02))
    print("  Walkthrough complete.")


# ── Runner ────────────────────────────────────────────────────────────────────
async def check_connection():
    uri = f"ws://{TARGET_HOST}:{TARGET_PORT}"
    try:
        async with websockets.connect(uri, compression=None, open_timeout=3):
            pass
        return True
    except Exception as e:
        print(f"  ✗ Cannot connect to {uri}: {e}")
        if TARGET_PORT == "8765":
            print("    → Start the simulator first:  python simulator_server.py")
            print("      Then open:                  http://localhost:8000")
        else:
            print(f"    → Ensure ESP32 is in Network Canvas mode.")
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
    verify_label = " [VERIFY]" if VERIFY_MODE else ""
    assertions_label = " + canvas assertions" if _using_simulator() else ""
    print(f"\nLeddite V2 Test Suite — {mode_label} @ {target}{verify_label}{assertions_label}")
    print("─" * 42)

    if not await check_connection():
        sys.exit(1)

    tests = [
        ("shapes",           test_shapes),
        ("text",             test_text),
        ("marquee",          test_marquee),
        ("bouncing_ball",    test_bouncing_ball),
        ("encoder_events",   test_encoder_events),
        ("canvas_assertions",test_canvas_assertions),
        ("verify_modes",     test_verify_modes),
    ]

    results = {}
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
        if name == "verify_modes" and not VERIFY_MODE:
            continue
        icon = "✓" if status == "PASS" else "✗"
        if status != "PASS":
            all_pass = False
        print(f"  {icon} {name:<22} {status}  ({elapsed:.2f}s)")

    print()
    if all_pass:
        print("All tests passed ✓")
    else:
        print("Some tests FAILED ✗")
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(run_all())
