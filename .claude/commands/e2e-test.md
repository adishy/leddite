# Leddite E2E Test Skill

Run a complete end-to-end test of the Leddite V2 system.
Accepts an optional argument: `simulator` (default), `hardware`, or an ESP32 IP address.

Usage examples:
  /e2e-test            — test against simulator
  /e2e-test hardware   — test against hardware (auto-detect IP from serial)
  /e2e-test 192.168.1.100  — test against specific ESP32 IP

---

## Step 1 — Native unit tests (always run first)

```bash
make test
```

All 31 tests must pass.  If any fail, stop here and fix them before continuing.

---

## Step 2 — Determine target

Parse the argument passed to this skill:
- No arg or "simulator" → target = `localhost:8765`
- "hardware" → try to detect the ESP32 IP from Serial output (see below)
- An IP address (e.g. 192.168.1.100) → target = `<ip>:81`

To detect ESP32 IP from serial output:
```bash
# Read 5 seconds of serial output at 115200 baud and grep for IP line
timeout 5 cat /dev/cu.usbserial-0001 2>/dev/null | grep -oE "IP: [0-9.]+" | head -1
```
If IP is found, use it with port 81.  If not, ask the user to provide the IP manually.

---

## Step 3 — Start simulator server (simulator target only)

If targeting the simulator:
```bash
# Check if already running
lsof -ti :8765 2>/dev/null && echo "already running" || true

# Start if not running
.venv/bin/python simulator_server.py &
SIM_PID=$!
sleep 1.5
```

Tell the user to open http://localhost:8000 in their browser to watch the tests live.

---

## Step 4 — Run test suite

### Simulator:
```bash
.venv/bin/python test_suite.py
```

### Hardware (replace IP):
```bash
.venv/bin/python test_suite.py <ESP32_IP> 81
```

Expected output: all tests showing "OK" or "PASS", with final "All tests passed ✓".

If any test fails, print the full error and suggest:
1. Check the ESP32 serial log for errors (open Arduino IDE serial monitor at 115200 baud)
2. Verify the ESP32 is in **Network Canvas** mode (NT in boot menu)
3. Re-run `/work-setup` to verify the environment

---

## Step 5 — Visual verify mode (optional, recommended for hardware)

Run with `--verify` to pause between tests for human / agent visual inspection:

```bash
.venv/bin/python test_suite.py <target_ip> 81 --verify
# or for simulator:
.venv/bin/python test_suite.py --verify
```

For each pause:
- Read the description printed to the console
- Confirm the display looks correct (browser simulator or physical hardware)
- Type ENTER to advance, or 'f' + ENTER to flag a failure

---

## Step 6 — Encoder verification (hardware only)

When testing hardware in Network Canvas mode, physically interact with the encoder
while the `test_encoder_events` test is running (it listens for 2 seconds).

Expected JSON events in the terminal:
- Turn CW:  `{"type": "encoder", "delta": 1}`
- Turn CCW: `{"type": "encoder", "delta": -1}`
- Press:    `{"type": "encoder", "button": "pressed"}`

---

## Step 7 — Stop simulator server (if started in step 3)

```bash
kill $SIM_PID 2>/dev/null || true
# Or kill by port if PID was lost:
lsof -ti :8765 | xargs kill -9 2>/dev/null || true
```

---

## Step 8 — Report

Print a final summary:
- Native unit test count and pass/fail
- Each e2e test name and result
- Any encoder events captured
- FPS from bouncing ball test
- Overall PASS / FAIL

If everything passes: confirm the system is healthy.
If anything fails: list the failing tests with their error messages and suggest next steps.
