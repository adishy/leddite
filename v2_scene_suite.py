import asyncio
from leddite_client import LedditeClient
from dsl_runner import LedditeDSLRunner

SCENES = {
    "weather_storm": """
CLEAR
# Background dark grey
RECT 0 0 16 16 20 20 40
# Cloud
RECT 4 4 8 4 100 100 120
PIXEL 5 3 100 100 120
PIXEL 6 3 100 100 120
PIXEL 9 3 100 100 120
PIXEL 10 3 100 100 120
# Lightning
PIXEL 7 8 255 255 0
PIXEL 6 9 255 255 0
PIXEL 7 9 255 255 0
PIXEL 5 10 255 255 0
SHOW
SLEEP 0.2
# Flash
RECT 0 0 16 16 255 255 200
SHOW
SLEEP 0.1
CLEAR
RECT 0 0 16 16 20 20 40
RECT 4 4 8 4 100 100 120
SHOW
SLEEP 2
""",
    "mario_coin": """
CLEAR
# Gold coin shape
RECT 6 4 4 8 255 200 0
RECT 5 5 6 6 255 200 0
# Shine
PIXEL 6 5 255 255 255
PIXEL 7 5 255 255 255
# Detail
RECT 7 6 2 4 200 150 0
SHOW
TEXT "1UP" 4 10 0 255 0
SLEEP 3
""",
    "pulsing_heart": """
CLEAR
LOOP 5
  # Heart On
  RECT 6 7 4 3 255 0 0
  PIXEL 7 10 255 0 0
  PIXEL 8 10 255 0 0
  SHOW
  SLEEP 0.5
  # Heart Off
  CLEAR
  SHOW
  SLEEP 0.5
ENDLOOP
"""
}

async def run_scenes():
    client = LedditeClient()
    await client.connect()
    runner = LedditeDSLRunner(client)
    
    print("Running Hardcoded Scene Test Suite...")
    for name, dsl in SCENES.items():
        print(f"Playing Scene: {name}")
        await runner.run_script(dsl)
        await asyncio.sleep(1)
    
    await client.close()
    print("Test Suite Complete.")

if __name__ == "__main__":
    asyncio.run(run_scenes())
