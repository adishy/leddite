# DSLRunner

**Role:** Interpreter for a simple command-based language to control the LED matrix.

## Key Facts
- **Language:** Line-based commands.
- **Commands:**
    - `CLEAR`: Reset screen.
    - `RECT x y w h r g b`: Draw rectangle.
    - `TEXT "msg" x y r g b [rot] [marquee]`: Draw text.
    - `SLEEP seconds`: Pause execution.
- **Integration:** Maps DSL commands directly to `LedditeClient` methods.
- **Purpose:** Provides a human-readable and LLM-friendly abstraction layer.

## Example Script
```text
CLEAR
RECT 0 0 16 16 10 0 20
TEXT "V2" 4 4 255 255 255
SLEEP 2
```
