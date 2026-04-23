import os
import asyncio
from google import genai
from dotenv import load_dotenv
from leddite_client import LedditeClient
from dsl_runner import LedditeDSLRunner

# Load .env.secrets from current dir or project root
load_dotenv(".env.secrets")
load_dotenv("../.env.secrets")
api_key = os.getenv("LLM_API_KEY")

if not api_key:
    print("Error: LLM_API_KEY not found in .env.secrets")
    exit(1)

# Initialize the new Google GenAI client
client = genai.Client(api_key=api_key)

SYSTEM_PROMPT = """
You are a creative artist for a 16x16 RGB LED Matrix called 'Leddite'.
You write code in a simple DSL (Domain Specific Language) to create 'scenes'.

DSL Commands:
- CLEAR : Clears the screen to black.
- RECT x y w h r g b : Draws a rectangle. (x,y is top-left, 0-15 range).
- TEXT "message" x y r g b [rotation] [marquee] : Draws text. 
    - rotation: 0=0, 1=90, 2=180, 3=270.
    - marquee: true/false.
- SLEEP seconds : Pauses execution.

Constraints:
- Matrix is exactly 16x16.
- (0,0) is top-left.
- Colors are 0-255.

Instructions:
Create a creative scene. Think of themes like 'Cyberpunk', 'Nature', 'Weather', or 'Retro Gaming'.
Provide ONLY the DSL code, one command per line. Do not include any other text or markdown.

Example:
CLEAR
RECT 0 0 16 16 20 0 40
TEXT "LEDDITE" 2 4 0 255 255 0 true
SLEEP 5
"""

async def generate_and_run_scene(theme):
    print(f"Generating scene for theme: '{theme}'...")
    
    # Try 2.0 Flash first, fallback to 1.5 Flash
    models_to_try = ['gemini-2.0-flash', 'gemini-1.5-flash']
    response = None
    
    for model_name in models_to_try:
        try:
            # Use the new SDK generation call
            response = client.models.generate_content(
                model=model_name,
                contents=f"{SYSTEM_PROMPT}\n\nTheme: {theme}"
            )
            print(f"Used model: {model_name}")
            break
        except Exception as e:
            if "404" in str(e) or "not found" in str(e).lower():
                continue
            else:
                raise e
    
    if not response:
        print("Error: Could not find a supported Gemini model.")
        return

    dsl_code = response.text.strip()
    
    # Remove markdown code blocks if the LLM added them
    if dsl_code.startswith("```"):
        lines = dsl_code.splitlines()
        if lines[0].startswith("```"): lines = lines[1:]
        if lines and lines[-1].startswith("```"): lines = lines[:-1]
        dsl_code = "\n".join(lines)

    print("--- Generated DSL ---")
    print(dsl_code)
    print("---------------------")

    leddite_client = LedditeClient()
    await leddite_client.connect()
    runner = LedditeDSLRunner(leddite_client)
    
    await runner.run_script(dsl_code)
    await leddite_client.close()

if __name__ == "__main__":
    import sys
    theme = "Cyberpunk City"
    if len(sys.argv) > 1:
        theme = " ".join(sys.argv[1:])
    
    asyncio.run(generate_and_run_scene(theme))
