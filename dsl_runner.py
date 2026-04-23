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

        elif cmd == "PIXEL":
            # PIXEL x y r g b
            x, y = map(int, parts[1:3])
            color = tuple(map(int, parts[3:6]))
            await self.client.set_pixel(x, y, color)

        elif cmd == "SHOW":
            # Force show()
            await self.client.send_sprite(1, 1, x=0, y=0, pixels=[0,0,0], flags=2)

        elif cmd == "TEXT":
            # TEXT "msg" x y r g b [rotation] [marquee]
            first_quote = line.find('"')
            last_quote = line.rfind('"')
            msg = line[first_quote+1:last_quote]
            remaining = line[last_quote+1:].strip().split()
            
            x, y = map(int, remaining[0:2])
            color = tuple(map(int, remaining[2:5]))
            rotation = int(remaining[5]) if len(remaining) > 5 else 0
            marquee = remaining[6].lower() == "true" if len(remaining) > 6 else False
            
            await self.client.write_text(msg, x, y, color, rotation, marquee)

        elif cmd == "SLEEP":
            await asyncio.sleep(float(parts[1]))

    async def run_script(self, script: str):
        lines = [line.strip() for line in script.splitlines() if line.strip()]
        i = 0
        while i < len(lines):
            line = lines[i]
            parts = line.split()
            cmd = parts[0].upper()

            if cmd == "LOOP":
                count_str = parts[1].upper()
                is_infinite = count_str in ["0", "INF", "INFINITY"]
                count = 0 if is_infinite else int(count_str)
                
                # Find the matching ENDLOOP
                loop_start = i + 1
                loop_end = -1
                nesting = 1
                for j in range(loop_start, len(lines)):
                    if lines[j].upper().startswith("LOOP"):
                        nesting += 1
                    elif lines[j].upper() == "ENDLOOP":
                        nesting -= 1
                        if nesting == 0:
                            loop_end = j
                            break
                
                if loop_end != -1:
                    loop_body = "\n".join(lines[loop_start:loop_end])
                    if is_infinite:
                        while True:
                            await self.run_script(loop_body)
                    else:
                        for _ in range(count):
                            await self.run_script(loop_body)
                    i = loop_end + 1
                    continue
                else:
                    print(f"Error: Missing ENDLOOP for LOOP at line {i}")
            
            elif cmd == "ENDLOOP":
                # Should have been handled by the LOOP case
                pass
            else:
                await self.execute_line(line)
            
            i += 1
