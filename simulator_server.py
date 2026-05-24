"""Leddite V2 Simulator Server

Combined HTTP + WebSocket server:
  HTTP  → http://localhost:8000              serves simulator/ directory
  HTTP  → http://localhost:8000/canvas-state returns current 16×16 canvas as JSON
  WS    → ws://localhost:8765               relays binary sprite frames + JSON encoder events

All connected clients act as peers — any message received from one client is
broadcast to all others.

The server maintains a Python-native canvas mirror (Canvas + Transformer exact
port) so /canvas-state always reflects the latest rendered frame, even when the
browser simulator is not open.  Tests can query this endpoint to make pixel-level
assertions without needing vision or browser automation.
"""

import asyncio
import http.server
import json
import os
import socketserver
import struct
import threading

import websockets

# ── Canvas mirror (Python port of Canvas.cpp + Transformer.cpp) ───────────────

CANVAS_W = 16
CANVAS_H = 16

# Flat list of [r, g, b, r, g, b, ...] — 256 pixels × 3 bytes
_canvas_lock = threading.Lock()
_canvas = bytearray(CANVAS_W * CANVAS_H * 3)


def _transform(sx, sy, w, h, rotation):
    """Port of Transformer::transformCoords — returns (tx, ty)."""
    if rotation == 1:   # 90°
        return h - 1 - sy, sx
    elif rotation == 2: # 180°
        return w - 1 - sx, h - 1 - sy
    elif rotation == 3: # 270°
        return sy, w - 1 - sx
    else:               # 0°
        return sx, sy


def _canvas_clear():
    with _canvas_lock:
        for i in range(len(_canvas)):
            _canvas[i] = 0


def _canvas_draw_sprite(data, w, h, x_off, y_off, rotation, clear_before):
    """Port of Canvas::drawSprite."""
    with _canvas_lock:
        if clear_before:
            for i in range(len(_canvas)):
                _canvas[i] = 0
        for sy in range(h):
            for sx in range(w):
                tx, ty = _transform(sx, sy, w, h, rotation)
                px = tx + x_off
                py = ty + y_off
                if px < 0 or px >= CANVAS_W or py < 0 or py >= CANVAS_H:
                    continue
                src = (sy * w + sx) * 3
                dst = (py * CANVAS_W + px) * 3
                _canvas[dst]     = data[src]
                _canvas[dst + 1] = data[src + 1]
                _canvas[dst + 2] = data[src + 2]


def _process_packet(raw: bytes):
    """Parse a binary protocol packet and update the canvas mirror."""
    if len(raw) < 8:
        return
    version  = raw[0]
    flags    = raw[1]
    w        = raw[2]
    h        = raw[3]
    x_off    = raw[4] if raw[4] <= 127 else raw[4] - 256  # int8
    y_off    = raw[5] if raw[5] <= 127 else raw[5] - 256
    rotation = raw[6]
    # raw[7] = brightness (not applied server-side)

    clear_before = bool(flags & 0x01)
    is_marquee   = bool(flags & 0x04)

    payload = raw[8:]
    expected = w * h * 3
    if len(payload) < expected:
        return

    if is_marquee:
        # For marquee packets, just draw the first frame at x=16 (right edge)
        # The browser handles the animated scroll; server tracks static state.
        _canvas_draw_sprite(payload, w, h, 16, y_off, rotation, clear_before)
    else:
        _canvas_draw_sprite(payload, w, h, x_off, y_off, rotation, clear_before)


def canvas_state_json() -> str:
    """Return the current canvas as a JSON object.

    Format:
      {
        "width": 16,
        "height": 16,
        "pixels": [[r,g,b], [r,g,b], ...]   // 256 entries, row-major (y=0 first)
      }
    """
    with _canvas_lock:
        pixels = [
            [_canvas[i * 3], _canvas[i * 3 + 1], _canvas[i * 3 + 2]]
            for i in range(CANVAS_W * CANVAS_H)
        ]
    return json.dumps({"width": CANVAS_W, "height": CANVAS_H, "pixels": pixels})


# ── Connected WS clients ──────────────────────────────────────────────────────
CLIENTS: set = set()
CLIENTS_LOCK = asyncio.Lock()


async def broadcast(sender, message):
    async with CLIENTS_LOCK:
        targets = [c for c in CLIENTS if c != sender]
    if not targets:
        return
    await asyncio.gather(
        *[_safe_send(c, message) for c in targets], return_exceptions=True
    )


async def _safe_send(client, message):
    try:
        await client.send(message)
    except websockets.exceptions.ConnectionClosed:
        pass


# ── WebSocket handler ─────────────────────────────────────────────────────────
CANVAS_ACK_MAGIC = 0xCA
FLAG_ACK_CANVAS  = 0x08


def _build_canvas_ack() -> bytes:
    """Build the 771-byte canvas readback response from the current mirror state."""
    with _canvas_lock:
        return bytes([CANVAS_ACK_MAGIC, CANVAS_W, CANVAS_H]) + bytes(_canvas)


async def register(websocket):
    async with CLIENTS_LOCK:
        CLIENTS.add(websocket)
    client_id = id(websocket)
    print(f"[WS] Client connected  #{client_id}  (total: {len(CLIENTS)})")
    try:
        async for message in websocket:
            if isinstance(message, bytes):
                _process_packet(message)
                print(f"[WS] Binary {len(message)}B from #{client_id}")
                # FLAG_ACK_CANVAS (0x08): reply to requester with current canvas state.
                # Mirrors ESP32 NetworkMode::sendCanvasAck — identical response format.
                if len(message) >= 2 and (message[1] & FLAG_ACK_CANVAS):
                    ack = _build_canvas_ack()
                    await _safe_send(websocket, ack)
                    print(f"[WS] Canvas ACK → #{client_id} ({len(ack)}B)")
            else:
                try:
                    ev = json.loads(message)
                    print(f"[WS] JSON from #{client_id}: {ev}")
                except json.JSONDecodeError:
                    print(f"[WS] Text from #{client_id}: {message[:80]}")
            await broadcast(websocket, message)
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        async with CLIENTS_LOCK:
            CLIENTS.discard(websocket)
        print(f"[WS] Client disconnected #{client_id}  (total: {len(CLIENTS)})")


# ── HTTP handler — serves simulator/ + /canvas-state ────────────────────────
class SimulatorHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        base_path = os.path.join(script_dir, "simulator")
        super().__init__(*args, directory=base_path, **kwargs)

    def do_GET(self):
        if self.path == "/canvas-state":
            body = canvas_state_json().encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)
        else:
            super().do_GET()

    def log_message(self, fmt, *args):
        if args and str(args[1]) not in ("200", "304"):
            super().log_message(fmt, *args)


class _ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


def run_http_server():
    with _ReusableTCPServer(("", 8000), SimulatorHandler) as httpd:
        print("[HTTP] Simulator  at http://localhost:8000")
        print("[HTTP] Canvas API at http://localhost:8000/canvas-state")
        httpd.serve_forever()


# ── Main ──────────────────────────────────────────────────────────────────────
async def main():
    http_thread = threading.Thread(target=run_http_server, daemon=True)
    http_thread.start()

    async with websockets.serve(register, "localhost", 8765, reuse_address=True):
        print("[WS]  Relay server at ws://localhost:8765")
        print("      Open http://localhost:8000 in your browser.")
        print("      Run: python test_suite.py          (simulator)")
        print("      Run: python test_suite.py <ip> 81  (hardware)")
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[Server] Stopped.")
