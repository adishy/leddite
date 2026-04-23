import os
import asyncio
import sys
from google import genai
from google.genai import errors
from dotenv import load_dotenv
from leddite_client import LedditeClient
from dsl_runner import LedditeDSLRunner

# Load .env.secrets from current dir or project root
load_dotenv(".env.secrets")
load_dotenv("../.env.secrets")
api_key = os.getenv("LLM_API_KEY")

SYSTEM_PROMPT = """
You are an expert Pixel Artist specializing in ultra-low-resolution 16x16 RGB LED displays.
The platform is 'Leddite', a 16x16 matrix (256 total pixels).

DSL Commands:
- CLEAR : Clears the screen to black.
- PIXEL x y r g b : Sets a single pixel. Best for fine detail.
- RECT x y w h r g b : Draws a rectangle. (x,y is top-left, range 0-15).
- TEXT "message" x y r g b [rotation] [marquee] : Draws 5x7 bitmap text.
    - rotation: 0=0, 1=90, 2=180, 3=270.
    - marquee: true/false. (Text must be longer than 16px to marquee).
- SLEEP seconds : Pauses execution.
- SHOW : Forces the display to refresh (useful after many PIXEL commands).
- LOOP count : Starts a loop that repeats 'count' times.
- ENDLOOP : Ends a loop block.

Pixel Art Constraints (CRITICAL):
1. Resolution is ONLY 16x16. Complex icons will look like blobs.
2. Use bold, contrasting colors.
3. For small icons, use 3x3 or 5x5 blocks.
4. (0,0) is top-left, (15,15) is bottom-right.

Instructions:
Create a creative, recognizable scene based on the theme. 
Use a combination of RECT for backgrounds, PIXEL for detail/icons, and TEXT for labels.
Provide ONLY the DSL code, one command per line. No markdown blocks.

Example (Retro Night):
CLEAR
RECT 0 0 16 16 20 0 40
RECT 2 2 12 12 0 0 0
PIXEL 7 7 255 255 0
PIXEL 8 7 255 255 0
TEXT "VIBE" 3 10 255 0 255
SLEEP 5
"""

MOCK_DSL = """
CLEAR
RECT 0 0 16 16 30 0 10
# Draw a simple heart with PIXEL
PIXEL 7 7 255 0 0
PIXEL 8 7 255 0 0
PIXEL 6 8 255 0 0
PIXEL 7 8 255 0 0
PIXEL 8 8 255 0 0
PIXEL 9 8 255 0 0
PIXEL 7 9 255 0 0
PIXEL 8 9 255 0 0
PIXEL 7 10 255 0 0
SHOW
TEXT "MOCK MODE" 0 2 255 255 255 0 true
SLEEP 5
"""

async def generate_and_run_scene(theme, mock=False):
    if mock:
        print("Running in MOCK mode (no API credits used)...")
        dsl_code = MOCK_DSL.strip()
    else:
        if not api_key:
            print("Error: LLM_API_KEY not found in .env.secrets")
            return

        print(f"Generating scene for theme: '{theme}'...")
        client = genai.Client(api_key=api_key)
        
        # Try latest models available in May 2026
        models_to_try = ['gemini-3.1-flash-lite-preview', 'gemini-3-flash']
        dsl_code = None
        
        for model_name in models_to_try:
            try:
                print(f"Trying model: {model_name}...")
                response = client.models.generate_content(
                    model=model_name,
                    contents=f"{SYSTEM_PROMPT}\n\nTheme: {theme}\nProvide ONLY the DSL code."
                )
                dsl_code = response.text.strip()
                print(f"Success with model: {model_name}")
                break
            except errors.ClientError as e:
                if "RESOURCE_EXHAUSTED" in str(e):
                    print("\n[!] ERROR: API Credits Depleted.")
                    print("Please top up at: https://ai.studio.google.com/")
                    print("Use --mock to test without credits: python llm_scenes.py --mock 'Theme'")
                    return
                elif "404" in str(e) or "NOT_FOUND" in str(e):
                    print(f"Model {model_name} not available, trying next...")
                    continue
                else:
                    raise e
        
        if not dsl_code:
            print("Error: No supported Gemini models found.")
            return

    # Cleanup markdown
    if dsl_code.startswith("```"):
        lines = dsl_code.splitlines()
        if lines[0].startswith("```"): lines = lines[1:]
        if lines and lines[-1].startswith("```"): lines = lines[:-1]
        dsl_code = "\n".join(lines)

    print("--- DSL Code ---")
    print(dsl_code)
    print("----------------")

    leddite_client = LedditeClient()
    try:
        await leddite_client.connect()
        runner = LedditeDSLRunner(leddite_client)
        await runner.run_script(dsl_code)
        await leddite_client.close()
    except Exception as e:
        print(f"Error connecting to simulator: {e}. Is 'make run-sim' active?")

if __name__ == "__main__":
    args = sys.argv[1:]
    is_mock = "--mock" in args
    if is_mock: args.remove("--mock")
    
    theme = " ".join(args) if args else "Cyberpunk City"
    asyncio.run(generate_and_run_scene(theme, mock=is_mock))
