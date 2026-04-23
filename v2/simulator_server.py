import asyncio
import http.server
import socketserver
import threading
import websockets
import struct
import os

# --- Protocol Helpers (from test_client.py) ---
def create_packet(width, height, x=0, y=0, rotation=0, flags=0, brightness=255, pixels=None):
    header = struct.pack('BBBBbbBB', 1, flags, width, height, x, y, rotation, brightness)
    if pixels is None:
        pixels = [255, 255, 255] * (width * height)
    return header + bytes(pixels)

# --- WebSocket Server ---
CLIENTS = set()

async def register(websocket):
    CLIENTS.add(websocket)
    try:
        async for message in websocket:
            # Broadcast incoming messages from test programs to all clients
            if CLIENTS:
                # Use wait to avoid blocking the main loop if one client is slow
                # Create tasks for all OTHER clients
                others = [c for c in CLIENTS if c != websocket]
                if others:
                    await asyncio.wait([asyncio.create_task(c.send(message)) for c in others])
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        CLIENTS.remove(websocket)

async def broadcast_pattern():
    """Background loop (optional, disabled by default to see test suite)."""
    # hue_offset = 0
    # while True:
    #     if CLIENTS:
    #         hue_offset = (hue_offset + 1) % 16
    #         pixels = []
    #         for y in range(16):
    #             for x in range(16):
    #                 r = (x * 16 + hue_offset * 10) % 256
    #                 g = (y * 16) % 256
    #                 b = 128
    #                 pixels.extend([r, g, b])
    #         packet = create_packet(16, 16, pixels=pixels, flags=0)
    #         for ws in CLIENTS:
    #             await ws.send(packet)
    #     await asyncio.sleep(0.05)
    pass

# --- HTTP Server ---
class SimulatorHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        # Serve from v2/simulator directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        base_path = os.path.join(script_dir, 'simulator')
        super().__init__(*args, directory=base_path, **kwargs)

def run_http_server():
    with socketserver.TCPServer(("", 8000), SimulatorHandler) as httpd:
        print("Simulator HTTP Server at http://localhost:8000")
        httpd.serve_forever()

# --- Main ---
async def main():
    # Start HTTP server in a separate thread
    http_thread = threading.Thread(target=run_http_server, daemon=True)
    http_thread.start()

    # Start WebSocket server
    async with websockets.serve(register, "localhost", 8765):
        print("WebSocket Server at ws://localhost:8765")
        # await broadcast_pattern() # Keep this silent for now
        await asyncio.Future() # Wait forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
