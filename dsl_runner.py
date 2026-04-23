import asyncio
from leddite_client import LedditeClient

class LedditeDSLRunner:
    def __init__(self, client: LedditeClient):
        self.client = client

    async def execute_line(self, line: str):
        line = line.strip()
        if not line or line.startswith("#"):
            return

        parts = line.split()
        cmd = parts[0].upper()

        if cmd == "CLEAR":
            await self.client.clear()
        
        elif cmd == "RECT":
            # RECT x y w h r g b
            x, y, w, h = map(int, parts[1:5])
            color = tuple(map(int, parts[5:8]))
            await self.client.draw_rect(x, y, w, h, color)

        elif cmd == "TEXT":
            # TEXT "msg" x y r g b [rotation] [marquee]
            # Simple parser for quoted string
            first_quote = line.find('"')
            last_quote = line.rfind('"')
            msg = line[first_quote+1:last_quote]
            remaining = line[last_quote+1:].strip().split()
            
            x, y = map(int, remaining[0:2])
            color = tuple(map(int, remaining[2:5]))
            
            rotation = 0
            if len(remaining) > 5:
                rotation = int(remaining[5])
            
            marquee = False
            if len(remaining) > 6:
                marquee = remaining[6].lower() == "true"
            
            await self.client.write_text(msg, x, y, color, rotation, marquee)

        elif cmd == "SLEEP":
            # SLEEP seconds
            await asyncio.sleep(float(parts[1]))

    async def run_script(self, script: str):
        for line in script.splitlines():
            await self.execute_line(line)
