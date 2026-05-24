# Leddite Work-Setup Skill

Run this skill to set up or verify the full Leddite V2 development environment.

## What to do

Work through the following checklist in order.  For each item, check if it is already satisfied before attempting to install or configure.

---

### 1. Arduino IDE + arduino-cli

Check if Arduino IDE is installed:
```bash
ls /Applications/Arduino\ IDE.app 2>/dev/null && echo "FOUND" || echo "MISSING"
```

If missing: instruct the user to download Arduino IDE 2.x from https://www.arduino.cc/en/software and install it to /Applications/.

After IDE is present, verify arduino-cli is bundled:
```bash
ACLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$ACLI" version
```

Check for the required board package (esp32 by Espressif):
```bash
"$ACLI" board listall | grep "esp32:esp32:esp32"
```
If missing:
```bash
"$ACLI" core update-index
"$ACLI" core install esp32:esp32
```

Check for required libraries:
```bash
"$ACLI" lib list | grep -E "FastLED|WebSockets|ESP32Encoder"
```
If any are missing, install them:
```bash
"$ACLI" lib install "FastLED"
"$ACLI" lib install "WebSockets"
"$ACLI" lib install "ESP32Encoder"
```

---

### 2. WiFi credentials file

Check if the credentials file exists:
```bash
ls esp32_firmware/wifi_credentials.h
```

If missing, create it (ask the user for SSID and password if not obvious from context):
```cpp
#pragma once
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
```
Remind the user that this file is gitignored and should never be committed.

---

### 3. Python environment

Check if the venv exists and has the right packages:
```bash
ls .venv/bin/python 2>/dev/null && echo "FOUND" || echo "MISSING"
```

If missing:
```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

If present but packages might be stale:
```bash
.venv/bin/pip install -r requirements.txt --quiet
```

---

### 4. Emscripten (for WASM simulator)

Check if emcc is available:
```bash
which emcc 2>/dev/null && emcc --version | head -1 || echo "MISSING"
```

If missing, guide the user:
```
Emscripten is required to rebuild the WASM module (make simulator).
Install via:
  git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
  cd ~/emsdk
  ./emsdk install latest
  ./emsdk activate latest
  source ./emsdk_env.sh

Add the source line to your shell profile (~/.zshrc or ~/.bashrc) to
persist it across sessions.

After installation, reopen this terminal and re-run /work-setup.
```

If present, note the version for the record.

---

### 5. Native unit tests

Run the tests to confirm the C++ core compiles and passes natively:
```bash
make test
```

All tests must pass before any firmware work.

---

### 6. Pre-compiled WASM module

Check if the WASM module is present and reasonably fresh:
```bash
ls -lh simulator/leddite_wasm.js simulator/leddite_wasm.wasm 2>/dev/null
```

If missing or older than 7 days relative to changes in src/ or include/:
- If emcc is available: run `make simulator`
- If not: warn the user the simulator will not work until WASM is rebuilt

---

### 7. Verify simulator end-to-end

Start the simulator server in the background:
```bash
.venv/bin/python simulator_server.py &
SIM_PID=$!
sleep 1
```

Run the test suite against it:
```bash
.venv/bin/python test_suite.py
```

Stop the server:
```bash
kill $SIM_PID 2>/dev/null
```

---

### 8. Summary

After all checks, print a clear summary table:

| Component              | Status |
|------------------------|--------|
| Arduino IDE            | ✓/✗   |
| arduino-cli            | ✓/✗   |
| esp32 board package    | ✓/✗   |
| FastLED library        | ✓/✗   |
| WebSockets library     | ✓/✗   |
| ESP32Encoder library   | ✓/✗   |
| wifi_credentials.h     | ✓/✗   |
| Python venv            | ✓/✗   |
| emcc (WASM)            | ✓/✗   |
| WASM module            | ✓/✗   |
| Unit tests             | ✓/✗   |
| Simulator (e2e)        | ✓/✗   |

Point out any ✗ items with the specific fix command.
