---
name: flash-firmware
description: Build and flash the Leddite ESP32 firmware, then read the serial boot log to confirm it came up. Covers toolchain install, the src/ to esp32_firmware/ copy step, WiFi credential generation, compile, upload, and serial monitoring. Use whenever asked to flash, upload, or verify firmware on the device.
---

# Build and flash Leddite firmware

## 0. Toolchain (once per machine)

Check whether a real `arduino-cli` responds before installing — a same-named
binary from another package may shadow it (`gh` on this repo's Linux box is the
unrelated npm `node-gh`, and the confusion class is identical):

```bash
arduino-cli version || echo "need to install"
```

**macOS** — Arduino IDE 2.x bundles it; prefer that if the IDE is installed:

```bash
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
[ -x "$ARDUINO_CLI" ] || ARDUINO_CLI="$(command -v arduino-cli)"
```

**Either platform** — standalone install (works on macOS and Linux; the script
detects the OS and architecture, including Apple Silicon):

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
  | BINDIR="$HOME/.local/bin" sh
```

Then, on both:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install FastLED WebSockets ESP32Encoder ArduinoJson
```

All four libraries are required. `ArduinoJson` is for the weather client;
`WebSockets` is Links2004's arduinoWebSockets.

## 1. Sync the duplicated core modules

The Arduino IDE compiles only files beside the `.ino` and cannot see `../src`, so
core modules exist twice. **Always sync before building** or you will flash stale
logic that no test covered:

```bash
tools/sync-firmware-copies.sh
```

## 2. WiFi credentials

`esp32_firmware/wifi_credentials.h` is gitignored and must exist. Generate it
from `LEDDITE_SSID` / `LEDDITE_PASSWORD` — the script never echoes the values:

```bash
tools/gen-wifi-credentials.sh [path-to-env-file]
```

Do not print the file contents afterwards.

## 3. Compile

```bash
arduino-cli compile -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino
```

Takes ~2–4 minutes cold; run it in the background rather than letting it time out.

**Flash sits at ~90% of the default partition.** If a change overflows it:

```bash
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs esp32_firmware/esp32_firmware.ino
```

## 4. Upload

Find the port — the naming differs by platform:

```bash
# Linux:  /dev/ttyUSB0        macOS: /dev/cu.usbserial-0001 (or /dev/cu.SLAB_USBtoUART)
PORT=$(ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART /dev/ttyUSB* 2>/dev/null | head -1)
arduino-cli board list        # cross-check
arduino-cli upload -p "$PORT" -b esp32:esp32:esp32 esp32_firmware/esp32_firmware.ino
```

On **Linux** the user must be in the `dialout` group (`id -nG | grep dialout`).
On **macOS** no group membership is needed, but a CP210x/CH34x driver may be
required for the USB-serial bridge if no `/dev/cu.*` device appears.

Upload takes ~20 s and ends with `Hash of data verified.`

## 5. Confirm it booted

Do not assume a successful upload means working firmware. Read the serial log.
`arduino-cli monitor` is awkward to drive non-interactively; use pyserial and
pulse RTS to reset the board so you capture the boot banner:

`timeout` is GNU coreutils — on macOS use `gtimeout` (from `brew install
coreutils`) or drop it, since the snippet self-terminates after 36 s anyway.

```bash
uv pip install pyserial
.venv/bin/python -c "
import serial,sys,time,glob
port = (glob.glob('/dev/cu.usbserial-*') + glob.glob('/dev/cu.SLAB_USBtoUART')
        + glob.glob('/dev/ttyUSB*'))[0]
s=serial.Serial(port,115200,timeout=1)
s.setDTR(False); s.setRTS(True); time.sleep(0.2); s.setRTS(False)
t0=time.time()
while time.time()-t0 < 36:
    l=s.readline()
    if l: sys.stdout.write(l.decode('utf-8','replace')); sys.stdout.flush()
"
```

A healthy boot looks like:

```
=== LEDDITE V2 Multi-Mode Firmware ===
Connecting to <ssid>....
IP: 192.168.0.113
NTP sync........ OK
Encoder ready (CLK=32, DT=33, SW=25)
WebSocket on ws://192.168.0.113:81
[UI] brightness level 4 -> FastLED 66 (worst case 3975 mA)
[UI] settings: brightness=4 place=CAMB unit=C
[Weather] client started for Cambridge, MA 02141
Boot menu: rotate encoder to navigate, press to select
[Weather] CAMB: 18.7C code=2 night
```

Check specifically for: an IP, NTP `OK`, the NVS settings line, and a
`[Weather] <CODE>: ...` line proving a live fetch succeeded. `[Weather] fetch
failed` with a backoff is a real failure, not noise.

## 6. Optional hardware e2e

`test_suite.py <ip> 81` requires the device to be in **Network Canvas** mode,
which needs physical encoder navigation. Say so rather than reporting the suite
as passing when it was never reachable.
