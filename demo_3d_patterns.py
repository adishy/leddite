"""demo_3d_patterns.py — visualise the 3D pattern suite in the browser simulator.

Renders the same 4 patterns as the new PatternMode C++ implementation,
sending binary frames over WebSocket to the simulator server.

Usage:
    # Start simulator first (in another terminal):
    #   .venv/bin/python simulator_server.py
    # Then:
    .venv/bin/python demo_3d_patterns.py [pattern_index]

    pattern_index: 0=SpinCube  1=StarWarp  2=Sparkle  3=OrbChase  (omit to cycle all)

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


# ── Pattern 0: Spinning Cube ──────────────────────────────────────────────────
#
# 3-phase 9-second cycle, colour changes each cycle:
#   Phase 1 (0-3s): Y-axis (horizontal) spin, fixed 42-unit X tilt for depth.
#   Phase 2 (3-6s): X-axis spin joins in; both axes rotate together.
#   Phase 3 (6-9s): scale grows 2→9, cube fills then engulfs the screen.
#
# Vertex encoding: bit0=x sign, bit1=y sign, bit2=z sign.
# Painter order: back face (dim) → laterals (mid) → front face (bright).

def draw_spinning_cube() -> bytearray:
    buf = bytearray(16 * 16 * 3)
    t = millis()

    CYCLE_MS  = 9000
    phase_ms  = t % CYCLE_MS
    cycle_idx = (t // CYCLE_MS) % 6
    hue       = cycle_idx * 42   # 6 distinct hues, step through rainbow

    # Continuous rotation — never freeze/jump at phase boundaries
    ang_y = (t / 20) * 2 * math.pi / 256

    if phase_ms < 3000:
        ang_x = 42 * 2 * math.pi / 256   # fixed tilt ~59° shows cube depth
        s = 5
    elif phase_ms < 6000:
        ang_x = (t / 30) * 2 * math.pi / 256   # X starts rotating
        s = 5
    else:
        ang_x = (t / 30) * 2 * math.pi / 256
        z     = phase_ms - 6000
        s     = min(9, 2 + int(z * 7 / 3000))

    cy, sy = math.cos(ang_y), math.sin(ang_y)
    cx, sx = math.cos(ang_x), math.sin(ang_x)

    # Project 8 vertices: index bit-mask → (±s, ±s, ±s)
    px_v = [0] * 8
    py_v = [0] * 8
    for i in range(8):
        vx = s if (i & 1) else -s
        vy = s if (i & 2) else -s
        vz = s if (i & 4) else -s

        rx  = vx * cy - vz * sy          # Y rotation
        rz  = vx * sy + vz * cy
        ry2 = vy * cx - rz * sx          # X rotation

        px_v[i] = int(8 + rx)
        py_v[i] = int(8 + ry2)

    r_d, g_d, b_d = hsv_rgb(hue, 220,  70)   # dim   — back face
    r_m, g_m, b_m = hsv_rgb(hue, 200, 140)   # mid   — laterals
    r_b, g_b, b_b = hsv_rgb(hue, 220, 220)   # bright — front face

    # Back face (bit2=0, z=-s: verts 0-3)
    for e0, e1 in [(0,1),(1,3),(3,2),(2,0)]:
        draw_line(buf, px_v[e0], py_v[e0], px_v[e1], py_v[e1], r_d, g_d, b_d)
    # Lateral edges
    for e0, e1 in [(0,4),(1,5),(2,6),(3,7)]:
        draw_line(buf, px_v[e0], py_v[e0], px_v[e1], py_v[e1], r_m, g_m, b_m)
    # Front face (bit2=1, z=+s: verts 4-7) — drawn last, on top
    for e0, e1 in [(4,5),(5,7),(7,6),(6,4)]:
        draw_line(buf, px_v[e0], py_v[e0], px_v[e1], py_v[e1], r_b, g_b, b_b)

    return buf


# ── Pattern 1: Spiral ─────────────────────────────────────────────────────────
#
# Single-arm outwardly-growing Archimedean spiral.
# The arm grows segment-by-segment from centre to full length (8 segments),
# holds briefly at full size, then resets — all while spinning continuously.
#
# ANGLE_STEP = 55 sin8-units ≈ 77° per segment → 8 segments ≈ 616° ≈ 1.7 turns.
# GROW_STEP_MS = 400 ms per segment → full arm in 3.2 s, holds 0.6 s, resets.
# Colour: hue shifts along arm (inner dim, outer bright), full-arm hue drifts.

def draw_spiral() -> bytearray:
    buf = bytearray(16 * 16 * 3)
    t = millis()

    # t_rot: matches C++ (uint8_t)(t/16) → full rotation ≈ 4.1s
    t_rot    = t * math.pi / 2048   # t/16 * 2π/256 = t*π/2048
    base_hue = (t // 31) % 256      # matches C++ (uint8_t)(t/31)

    N            = 8
    angle_step   = 55 * 2 * math.pi / 256   # 55 sin8-units in radians
    GROW_STEP_MS = 400
    HOLD_MS      = 600
    CYCLE_MS     = N * GROW_STEP_MS + HOLD_MS  # 3800 ms

    t_cycle   = t % CYCLE_MS
    n_visible = int(t_cycle / GROW_STEP_MS) + 1 if t_cycle < N * GROW_STEP_MS else N
    n_visible = min(n_visible, N)

    prev_x, prev_y = 8, 8
    for i in range(1, n_visible + 1):
        theta = t_rot + i * angle_step
        r     = i   # 1..8 pixels from centre

        x = int(8 + r * math.cos(theta))
        y = int(8 + r * math.sin(theta))

        bri = int(60 + 195 * i / N)        # 60→255 dim→bright
        hue = (base_hue + i * 9) % 256     # hue gradient along arm
        rc, gc, bc = hsv_rgb(hue, 230, bri)
        draw_line(buf, prev_x, prev_y, x, y, rc, gc, bc)

        prev_x, prev_y = x, y

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

    hue1 = (160 + (t // 900) % 40) & 0xFF
    hue2 = (220 + (t // 700) % 30) & 0xFF

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

PATTERN_NAMES = ["Spinning Cube", "Spiral", "Sparkle", "Orb Chase"]
PATTERN_FNS   = [draw_spinning_cube, draw_spiral, draw_sparkle, draw_orb_chase]
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
                print(f"Cycling all {len(PATTERN_FNS)} patterns, {PATTERN_DURATION:.0f}s each — Ctrl-C to stop")
                print(f"  → {PATTERN_NAMES[idx]}")
                while True:
                    if time.monotonic() - t_sw >= PATTERN_DURATION:
                        idx  = (idx + 1) % len(PATTERN_FNS)
                        t_sw = time.monotonic()
                        print(f"  → {PATTERN_NAMES[idx]}")
                        # Reset stateful patterns on switch
                        if idx == 2:
                            global _sparkle_map
                            _sparkle_map[:] = [0] * 256
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
