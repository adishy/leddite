# Leddite V2: High-Performance RGB Matrix API

Leddite V2 is a streamlined, "Sprite-based" binary API for controlling 16x16 RGB LED matrices (WS2812B) on an ESP32.

## Key Features
- **100% Logic Parity:** The C++ core is compiled to WebAssembly (WASM) for the JS Simulator.
- **Binary Protocol:** Optimized for 30+ FPS low-latency updates.
- **LLM Powered:** Generate creative scenes using natural language and the Gemini API.
- **Automated Marquee:** Internal engine for smooth text scrolling.

## Quick Start (Simulator)

1.  **Build and Start:**
    ```bash
    make run-sim
    ```
2.  **Open Simulator:** Visit `http://localhost:8000` in your browser.
3.  **Run LLM Scene:**
    - Create `.env.secrets` in the root and add `LLM_API_KEY=your_key`.
    - Run:
      ```bash
      .venv/bin/python llm_scenes.py "A rainy night in Tokyo"
      ```

## Development & Hardware
- **Unit Tests:** `make test`
- **Firmware:** Upload `esp32_firmware/esp32_firmware.ino` to your ESP32.
- **Documentation:** Explore the Obsidian vault in `docs/` for deep-dives into each component.

## Project Structure
- `src/`, `include/`: Core C++ logic.
- `esp32_firmware/`: Production ESP32 code.
- `simulator/`: WASM-powered browser simulator.
- `hardware_build/`, `hw_setup/`: CAD files, datasheets, and physical setup guides.
- `docs/`: Obsidian documentation vault.
