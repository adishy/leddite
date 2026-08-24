#!/usr/bin/env python3
"""Render captured Leddite frames as contact sheets, and measure what is
measurable about them.

    python3 tools/ux/review.py frames.json out-dir/

Produces, per scene, a PNG contact sheet (or a filmstrip for animated scenes),
plus a metrics table on stdout.

WHY BOTH A PICTURE AND NUMBERS
------------------------------
The numbers catch what is objective and easy to get wrong without noticing:
contrast too low to read, content bleeding into a layout gap, a frame that is
nearly blank, a scroll that never moves. They cannot judge whether a glyph is
*recognisable*, whether a colour reads as "rain", or whether a layout is
balanced. That needs eyes on the sheet. Neither half is sufficient alone, which
is why this script emits both and the ux-review skill requires looking at the
image rather than stopping at the table.
"""

import json
import sys
from collections import OrderedDict
from pathlib import Path

from PIL import Image, ImageDraw

W = H = 16
SCALE = 14          # px per LED in the rendered sheet
PAD = 10
LABEL_H = 15
COLS = 5

BG = (24, 24, 28)
LED_OFF = (17, 17, 20)
GRID = (10, 10, 12)
TEXT = (214, 214, 224)


# ── Pixel helpers ─────────────────────────────────────────────────────────────

def px(frame, x, y):
    i = (y * W + x) * 3
    return tuple(frame[i:i + 3])


def lit(c):
    return c != (0, 0, 0)


def luma(c):
    # Rec. 601 — good enough for judging whether text separates from its ground.
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def row_cells(frame, y):
    return [px(frame, x, y) for x in range(W)]


# ── Metrics ───────────────────────────────────────────────────────────────────

def fill_ratio(frame):
    return sum(1 for y in range(H) for x in range(W) if lit(px(frame, x, y))) / (W * H)


def distinct_colors(frame):
    return len({px(frame, x, y) for y in range(H) for x in range(W) if lit(px(frame, x, y))})


def band_contrast(frame):
    """Best text/background luma separation on any 4px row band, or None.

    Menus draw the selected label as black knocked out of a coloured band, so
    the meaningful measure is the luma gap between the band and the holes in it.
    Returns None when no band is present — a full-bleed game frame has no
    background to separate from, and reporting 0 there reads as "no contrast"
    when the truth is "not applicable".
    """
    best = None
    for top in range(0, 13, 5):
        cells = [px(frame, x, y) for y in range(top, min(top + 4, H)) for x in range(W)]
        on = [c for c in cells if lit(c)]
        off = [c for c in cells if not lit(c)]
        # Require enough of each that this is text-in-a-band, not a stray pixel.
        if len(on) >= 8 and len(off) >= 8:
            gap = max(luma(c) for c in on)      # ground vs. black glyph holes
            best = gap if best is None else max(best, gap)
    return best


def gap_rows_clean(frame, gaps):
    """Rows that the layout reserves as separators must stay dark."""
    dirty = [y for y in gaps if any(lit(c) for c in row_cells(frame, y))]
    return dirty


# Rows the layout reserves as blank, per scene. A scene absent from here has
# none — which is the case for every menu: ListMenu's rows are 7 + 4 + 4 and fill
# the panel above the position bar exactly, with no gutters (docs/adr/0012).
SCENE_GAPS = {
    'weather.conditions': [4, 5, 6, 7, 15],
    'weather.temps':      [4, 5, 6, 7, 15],
    'weather.scroll':     [4, 5, 6, 7, 15],
}


def measure(scene, shots):
    rows = []
    for s in shots:
        f = s['px']
        dirty = gap_rows_clean(f, SCENE_GAPS.get(scene, []))
        rows.append({
            'label': s['label'],
            'fill': fill_ratio(f),
            'colors': distinct_colors(f),
            'peak': band_contrast(f),
            'gaps': dirty,
        })
    return rows


def movement(shots):
    """Animation health: how many consecutive frames differ, and how long the
    scene held still at the start.

    A run of identical opening frames is not a stall — both scrolling widgets
    dwell deliberately so the first characters can be read before the text moves.
    Only a scene that never moves at all is broken.
    """
    moved = sum(1 for a, b in zip(shots, shots[1:]) if a['px'] != b['px'])
    dwell = 0
    for a, b in zip(shots, shots[1:]):
        if a['px'] != b['px']:
            break
        dwell += 1
    return moved, max(len(shots) - 1, 1), dwell


# ── Rendering ─────────────────────────────────────────────────────────────────

def draw_panel(d, frame, ox, oy):
    for y in range(H):
        for x in range(W):
            c = px(frame, x, y)
            x0, y0 = ox + x * SCALE, oy + y * SCALE
            d.rectangle([x0, y0, x0 + SCALE - 1, y0 + SCALE - 1], fill=GRID)
            d.ellipse([x0 + 1, y0 + 1, x0 + SCALE - 2, y0 + SCALE - 2],
                      fill=c if lit(c) else LED_OFF)


def sheet(shots, path, cols=COLS):
    cell_w = W * SCALE + PAD * 2
    cell_h = H * SCALE + PAD + LABEL_H
    rows = (len(shots) + cols - 1) // cols
    img = Image.new('RGB', (cell_w * cols, cell_h * rows), BG)
    d = ImageDraw.Draw(img)
    for n, s in enumerate(shots):
        cx, cy = (n % cols) * cell_w, (n // cols) * cell_h
        draw_panel(d, s['px'], cx + PAD, cy + LABEL_H)
        d.text((cx + PAD, cy + 2), s['label'][:26], fill=TEXT)
    img.save(path)
    return img.size


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    shots = json.load(open(sys.argv[1]))
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    scenes = OrderedDict()
    for s in shots:
        scenes.setdefault(s['scene'], []).append(s)

    print(f"{len(shots)} frames across {len(scenes)} scenes\n")
    written = []

    for scene, group in scenes.items():
        animated = group[0].get('strip') is not None
        # Filmstrips read in time order, so they get one long row.
        path = out / f"{scene.replace('.', '_')}.png"
        size = sheet(group, path, cols=len(group) if animated else COLS)
        written.append(path)

        print(f"── {scene}  ({len(group)} frames, {size[0]}x{size[1]})")
        if animated:
            moved, total, dwell = movement(group)
            flag = '   <-- NEVER MOVES' if moved == 0 else ''
            dwell_ms = group[dwell]['t'] - group[0]['t'] if dwell else 0
            print(f"   animation: {moved}/{total} consecutive frames differ, "
                  f"dwell {dwell_ms}ms{flag}")

        for m in measure(scene, group):
            warn = []
            # Sparse by design is normal for games (Snake is a handful of lit
            # pixels); a frame with almost nothing on it at all is not.
            if m['fill'] < 0.015:
                warn.append('NEARLY BLANK')
            if m['gaps']:
                warn.append(f"BLEEDS INTO ROWS {m['gaps']}")
            if m['peak'] is not None and m['peak'] < 60:
                warn.append('BAND TOO DIM FOR BLACK TEXT')
            tail = ('   <-- ' + '; '.join(warn)) if warn else ''
            peak = f"{m['peak']:5.1f}" if m['peak'] is not None else "    -"
            print(f"   {m['label'][:28]:<28} fill={m['fill']:>4.0%}  "
                  f"colors={m['colors']:<3} peak={peak}{tail}")
        print()

    print("Contact sheets written:")
    for p in written:
        print(f"  {p}")
    print("\nLook at every sheet. The table cannot tell you whether a glyph is")
    print("recognisable, whether a colour reads as its condition, or whether a")
    print("layout is balanced.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
