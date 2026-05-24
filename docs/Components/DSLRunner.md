# DSLRunner

**Role:** Interpreter for a simple command-based language to control the LED matrix.

## Key Facts
- **Language:** Line-based commands.
- **Commands:**
    - `CLEAR`: Reset screen.
    - `PIXEL x y r g b`: Set a single pixel (best for fine detail).
    - `RECT x y w h r g b`: Draw rectangle.
    - `TEXT "msg" x y r g b [rot] [marquee]`: Draw text.
    - `SLEEP seconds`: Pause execution.
    - `SHOW`: Force a display refresh.
    - `LOOP count`: Repeats the following block 'count' times. Use `0`, `INF`, or `INFINITY` for infinite loops.
    - `ENDLOOP`: Ends a loop block.
- **Integration:** Maps DSL commands directly to `LedditeClient` methods.
- **Purpose:** Provides a human-readable and LLM-friendly abstraction layer.

## Example Script
```text
CLEAR
RECT 0 0 16 16 10 0 20
TEXT "V2" 4 4 255 255 255
SLEEP 2
```
