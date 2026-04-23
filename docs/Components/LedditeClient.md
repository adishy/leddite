# LedditeClient (Python)

**Role:** High-level Python library for interacting with the Leddite V2 binary protocol.

## Key Facts
- **Communication:** Uses WebSockets (`websockets` library) to send binary packets.
- **Protocol:** Implements the V2 8-byte header + RGB payload spec.
- **Features:**
    - `clear()`: Wipes the canvas.
    - `draw_rect()`: Draws solid color shapes.
    - `write_text()`: Renders 5x7 bitmap text (port of legacy font).
    - `send_sprite()`: Low-level access for raw pixel data.

## Usage
```python
client = LedditeClient()
await client.connect()
await client.write_text("HELLO", color=(255,0,0))
```
