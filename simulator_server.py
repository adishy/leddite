"""Leddite V2 Simulator Server

Combined HTTP + WebSocket server:
  HTTP  → http://localhost:8000   serves simulator/ directory
  WS    → ws://localhost:8765     relays binary sprite frames + JSON encoder events

All connected clients act as peers — any message received from one client is
broadcast to all others.  This means:
  • test_suite.py / leddite_client.py  →  sends binary frames  →  browser sees them
  • browser encoder buttons            →  sends JSON events    →  test suite can read them
  • hardware ESP32 (port 81)           →  separate connection; see test_suite.py --hw
"""

import asyncio
import http.server
import json
import os
import socketserver
import threading

import websockets

# ── Connected clients ─────────────────────────────────────────────────────────
CLIENTS: set = set()
CLIENTS_LOCK = asyncio.Lock()


async def broadcast(sender, message):
    """Send message to every client except the sender."""
    async with CLIENTS_LOCK:
        targets = [c for c in CLIENTS if c != sender]
    if not targets:
        return
    results = await asyncio.gather(
        *[_safe_send(c, message) for c in targets], return_exceptions=True
    )
    for target, result in zip(targets, results):
        if isinstance(result, Exception):
            # Client disconnected mid-broadcast; will be cleaned up by its handler
            pass


async def _safe_send(client, message):
    try:
        await client.send(message)
    except websockets.exceptions.ConnectionClosed:
        pass


# ── WebSocket handler ─────────────────────────────────────────────────────────
async def register(websocket):
    async with CLIENTS_LOCK:
        CLIENTS.add(websocket)
    client_id = id(websocket)
    print(f"[WS] Client connected  #{client_id}  (total: {len(CLIENTS)})")
    try:
        async for message in websocket:
            # Log type for visibility
            if isinstance(message, bytes):
                print(f"[WS] Binary frame {len(message)}B from #{client_id}")
            else:
                try:
                    ev = json.loads(message)
                    print(f"[WS] JSON frame from #{client_id}: {ev}")
                except json.JSONDecodeError:
                    print(f"[WS] Text frame from #{client_id}: {message[:80]}")
            await broadcast(websocket, message)
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        async with CLIENTS_LOCK:
            CLIENTS.discard(websocket)
        print(f"[WS] Client disconnected #{client_id}  (total: {len(CLIENTS)})")


# ── HTTP server ───────────────────────────────────────────────────────────────
class SimulatorHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        base_path = os.path.join(script_dir, "simulator")
        super().__init__(*args, directory=base_path, **kwargs)

    def log_message(self, fmt, *args):
        # Silence per-request HTTP noise; keep error-level logs
        if args and str(args[1]) not in ("200", "304"):
            super().log_message(fmt, *args)


class _ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


def run_http_server():
    with _ReusableTCPServer(("", 8000), SimulatorHandler) as httpd:
        print("[HTTP] Simulator at http://localhost:8000")
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
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[Server] Stopped.")
