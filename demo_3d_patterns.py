"""demo_3d_patterns.py — visualise the 3D pattern suite in the browser simulator.

Renders the same 4 patterns as the new PatternMode C++ implementation,
sending binary frames over WebSocket to the simulator server.

Usage:
    # Start simulator first (in another terminal):
    #   .venv/bin/python simulator_server.py
    # Then:
    .venv/bin/python demo_3d_patterns.py [pattern_index]

    pattern_index: 0=ZoomCube  1=SpinPyramid  2=Sparkle  3=OrbChase  (omit to cycle all)

Open http://localhost:8000 to see the output.
"""

import asyncio
import colorsys
import math
import random
import struct
import sys
import time

import websockets

# ── Helpers ───────────────────────────────────────────────────────────────────

_t0 = time.monotonic()

def millis() -> int:
    return int((time.monotonic() - _t0) * 1000)


def sin8(theta: int) -> int:
    """FastLED sin8 equivalent: theta 0-255 → 0-255, midpoint=128."""
    return int(math.sin(theta * 2 * math.pi / 256) * 127) + 128


def cos8(theta: int) -> int:
    return int(math.cos(theta * 2 * math.pi / 256) * 127) + 128


def hsv_rgb(h: int, s: int, v: int):
    """h, s, v in 0-255.  Returns (r, g, b) in 0-255."""
    r, g, b = colorsys.hsv_to_rgb(h / 255, s / 255, v / 255)
    return int(r * 255), int(g * 255), int(b * 255)


def set_pix(buf: bytearray, x: int, y: int, r: int, g: int, b: int):
    if 0 <= x < 16 and 0 <= y < 16:
        i = (y * 16 + x) * 3
        buf[i], buf[i+1], buf[i+2] = r, g, b


def draw_line(buf: bytearray, x0: int, y0: int, x1: int, y1: int,
              r: int, g: int, b: int):
    """Bresenham line — clips to 16×16."""
    dx =  abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        set_pix(buf, x0, y0, r, g, b)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0  += sx
        if e2 <= dx:
            err += dx
            y0  += sy


def make_packet(buf: bytearray) -> bytes:
    """Wrap a 16×16 RGB buffer in the Leddite binary protocol."""
    header = struct.pack('BBBBbbBB',
                         1,         # version
                         0x01|0x02, # clear + show
                         16, 16,    # width, height
                         0, 0,      # x, y offset
                         0,         # rotation
                         255)       # brightness
    return header + bytes(buf)


# ── Pattern 0: Zoom Cube ──────────────────────────────────────────────────────

def draw_zoom_cube() -> bytearray:
    """Five wireframe square rings expanding from centre → infinite tunnel."""
    buf = bytearray(16 * 16 * 3)
    t = millis()
    base_phase = (t // 30) & 0xFF

    for ring in range(5):
        phase = (base_phase + ring * 51) & 0xFF
        s = 1 + (phase >> 5)       # half-size 1–8

        bri = min(255, 40 + s * 27)
        hue = (160 + t // 400 + ring * 12) & 0xFF
        r, g, b = hsv_rgb(hue, 230, bri)

        x0, x1 = 8 - s, 7 + s
        y0, y1 = 8 - s, 7 + s

        draw_line(buf, x0, y0, x1, y0, r, g, b)  # top
        draw_line(buf, x0, y1, x1, y1, r, g, b)  # bottom
        draw_line(buf, x0, y0, x0, y1, r, g, b)  # left
        draw_line(buf, x1, y0, x1, y1, r, g, b)  # right

    return buf


# ── Pattern 1: Spin Pyramid ───────────────────────────────────────────────────

def draw_spin_pyramid() -> bytearray:
    """Rotating wireframe square pyramid, isometric projection."""
    buf = bytearray(16 * 16 * 3)
    t  = millis()
    th = (t // 20) & 0xFF

    # Apex: fixed top-centre
    ax, ay = 8, 3

    # Base: four corners rotating in X-Z plane, projected isometrically
    # screen_x = CX + (x3d - z3d) * 7/8
    # screen_y = CY + (x3d + z3d) / 2
    CX, CY = 8, 10
    BSCALE = 4

    corners = []
    for i in range(4):
        angle = (th + i * 64) & 0xFF
        x3d = ((cos8(angle) - 128) * BSCALE) >> 7
        z3d = ((sin8(angle) - 128) * BSCALE) >> 7
        bx = CX + ((x3d - z3d) * 7) // 8
        by = CY + (x3d + z3d) // 2
        corners.append((bx, by))

    hue = (128 + (t // 700) % 40) & 0xFF
    r_b, g_b, b_b = hsv_rgb(hue, 210, 210)  # bright
    r_d, g_d, b_d = hsv_rgb(hue, 210,  80)  # dim

    # Lateral edges
    for bx, by in corners:
        draw_line(buf, ax, ay, bx, by, r_b, g_b, b_b)

    # Base edges (dim)
    for i in range(4):
        bx0, by0 = corners[i]
        bx1, by1 = corners[(i + 1) & 3]
        draw_line(buf, bx0, by0, bx1, by1, r_d, g_d, b_d)

    return buf


# ── Pattern 2: Sparkle ────────────────────────────────────────────────────────

_sparkle_map = [0] * 256

def draw_sparkle() -> bytearray:
    """Twinkling pixels over dark navy background."""
    global _sparkle_map
    buf = bytearray(16 * 16 * 3)
    t = millis()

    # Dark navy background
    for y in range(16):
        for x in range(16):
            set_pix(buf, x, y, 0, 0, 15)

    # Spawn new sparkles
    new_count = 3 + (t // 100) % 3
    for _ in range(new_count):
        _sparkle_map[random.randint(0, 255)] = 255

    # Decay and render
    for i in range(256):
        if _sparkle_map[i] > 0:
            bri = _sparkle_map[i]
            _sparkle_map[i] = max(0, bri - 20)
            x, y = i % 16, i // 16
            set_pix(buf, x, y, bri, bri, min(255, bri + 40))

    return buf


# ── Pattern 3: Orb Chase ──────────────────────────────────────────────────────

def draw_orb_chase() -> bytearray:
    """Two colour orbs orbiting at different speeds (circles chasing circles)."""
    buf = bytearray(16 * 16 * 3)
    t = millis()

    a1  = (t // 28) & 0xFF
    o1x = 8 + ((cos8(a1) - 128) * 5) // 128
    o1y = 8 + ((sin8(a1) - 128) * 5) // 128

    a2  = (t // 15) & 0xFF
    o2x = 8 + ((cos8(a2) - 128) * 3) // 128
    o2y = 8 + ((sin8(a2) - 128) * 3) // 128

    hue1 = (160 + (t // 900) % 40) & 0xFF   # blue → violet
    hue2 = (220 + (t // 700) % 30) & 0xFF   # magenta → rose

    for y in range(16):
        for x in range(16):
            d1sq = (x - o1x)**2 + (y - o1y)**2
            d2sq = (x - o2x)**2 + (y - o2y)**2

            b1 = max(0, 220 - d1sq * 12) if d1sq < 30 else 0
            b2 = max(0, 220 - d2sq * 12) if d2sq < 30 else 0
            b1 = min(255, b1)
            b2 = min(255, b2)

            if b1 == 0 and b2 == 0:
                set_pix(buf, x, y, 0, 0, 8)
            elif b1 >= b2:
                set_pix(buf, x, y, *hsv_rgb(hue1, 240, b1))
            else:
                set_pix(buf, x, y, *hsv_rgb(hue2, 240, b2))

    return buf


# ── Runner ────────────────────────────────────────────────────────────────────

PATTERN_NAMES = ["Zoom Cube", "Spin Pyramid", "Sparkle", "Orb Chase"]
PATTERN_FNS   = [draw_zoom_cube, draw_spin_pyramid, draw_sparkle, draw_orb_chase]
PATTERN_DURATION = 12.0   # seconds per pattern when cycling


async def run(fixed_pattern: int | None = None):
    uri = "ws://localhost:8765"
    print(f"Connecting to simulator at {uri} …")
    try:
        async with websockets.connect(uri, compression=None) as ws:
            if fixed_pattern is not None:
                print(f"Pattern: {PATTERN_NAMES[fixed_pattern]}  (Ctrl-C to stop)")
                while True:
                    await ws.send(make_packet(PATTERN_FNS[fixed_pattern]()))
                    await asyncio.sleep(0.033)
            else:
                idx   = 0
                t_sw  = time.monotonic()
                print(f"Cycling all 4 patterns, {PATTERN_DURATION:.0f}s each — Ctrl-C to stop")
                print(f"  → {PATTERN_NAMES[idx]}")
                while True:
                    if time.monotonic() - t_sw >= PATTERN_DURATION:
                        idx  = (idx + 1) % len(PATTERN_FNS)
                        t_sw = time.monotonic()
                        print(f"  → {PATTERN_NAMES[idx]}")
                        _sparkle_map[:] = [0] * 256   # reset sparkle on switch
                    await ws.send(make_packet(PATTERN_FNS[idx]()))
                    await asyncio.sleep(0.033)

    except ConnectionRefusedError:
        print("Could not connect.  Is simulator_server.py running?")
        print("  → .venv/bin/python simulator_server.py")
        raise SystemExit(1)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    pin = None
    if len(sys.argv) > 1:
        pin = int(sys.argv[1])
        if not (0 <= pin < len(PATTERN_FNS)):
            print(f"Pattern index must be 0-{len(PATTERN_FNS)-1}")
            raise SystemExit(1)
    asyncio.run(run(pin))
