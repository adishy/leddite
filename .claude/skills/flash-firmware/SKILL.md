---
name: flash-firmware
description: Build and flash the Leddite ESP32 firmware over USB or over the air, then read the serial boot log to confirm it came up. Covers toolchain install, the src/ to esp32_firmware/ copy step, WiFi credential generation, compile, upload, the browser-upload OTA workflow and how to drive it from a script, and serial monitoring. Use whenever asked to flash, upload, update or verify firmware on the device.
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

**Always pass `PartitionScheme=min_spiffs`.** Omitting it silently reverts to the
smaller app slots, and the size check then measures against the wrong maximum.

```bash
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs esp32_firmware/esp32_firmware.ino
```

Takes ~2–4 minutes cold; run it in the background rather than letting it time out.
The build sits at ~62% of a `min_spiffs` app slot (`docs/adr/0015`).

## 4. Upload

Find the port — the naming differs by platform:

```bash
# Linux:  /dev/ttyUSB0        macOS: /dev/cu.usbserial-0001 (or /dev/cu.SLAB_USBtoUART)
PORT=$(ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART /dev/ttyUSB* 2>/dev/null | head -1)
arduino-cli board list        # cross-check
arduino-cli upload -p "$PORT" -b esp32:esp32:esp32:PartitionScheme=min_spiffs esp32_firmware/esp32_firmware.ino
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
Firmware 2.2.0 on app0
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

The `Firmware <version> on <slot>` line is what distinguishes a successful OTA
from a silent no-op. `(pending verify)` after the slot means the image is on
probation and will roll back unless it stays up for 30 s (`docs/adr/0015`).

## 5b. Which path: USB or OTA?

**Use OTA unless one of these applies.** It is faster (no cable, ~15 s of
transfer) and needs nothing but a browser.

| Use USB when | Why |
|---|---|
| The partition scheme changed | A partition table is not part of an OTA payload. A device on the old layout would write the image at the wrong offset. |
| The device has no WiFi | `Settings → IP` says `NO WIFI`, or `Settings → UPDATE → YES` shows `ERR` immediately. |
| You are flashing a `-DLEDDITE_*` test image | You want it gone again afterwards; see §6. |
| The device is bricked or rolled back to an unknown image | Start from a known state. |

## 5c. The OTA workflow

**The device does NOT fetch. You upload to it.** There is no image server, no
URL to configure, no `ota_config.h` (deleted), and no firewall rule. If you find
yourself setting up an HTTP server or debugging why the panel cannot reach the
build host, stop — you are following instructions that predate `docs/adr/0015`.

### By hand, which is what you tell a user to do

1. Build the image (§3), adding `--output-dir build/fw` so you have a `.bin` to
   hand:

   ```bash
   arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
     --output-dir build/fw esp32_firmware/esp32_firmware.ino
   ```

2. At the panel: `Settings → UPDATE`. It opens on **NO** — turn to **YES** and
   press. The panel scrolls the URL to browse to, e.g. `HTTP://192.168.0.113`.
   The knob is inverted and takes **two clicks per row** (`docs/adr/0016`), which
   surprises people who have used it before.

3. Open that URL in a browser on the same network, drop
   `build/fw/esp32_firmware.ino.bin` on the page, press **Install**.

4. The panel shows a percentage, then `OK`, then reboots.

The window closes itself after five minutes, so tell the user to open it when
they are ready to upload, not in advance. It also closes on the next press, on a
long-press, and on completion.

### From a script, with nobody at the encoder

`-DLEDDITE_TEST_OTA_ON_BOOT` walks the real menu to UPDATE, confirms YES and
leaves the window open at the end of `setup()`. Flash that image over USB once,
then POST to it as many times as you like:

```bash
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --build-property "compiler.cpp.extra_flags=-DLEDDITE_TEST_OTA_ON_BOOT" \
  --output-dir build/fw-seam esp32_firmware/esp32_firmware.ino
arduino-cli upload -p /dev/ttyUSB0 -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --input-dir build/fw-seam esp32_firmware/esp32_firmware.ino

curl -sf -F "f=@build/fw/esp32_firmware.ino.bin" http://<ip>/update    # prints OK
```

The window only reopens on a reboot, so reset the board between runs.

Two other endpoints, useful for checking the device is in the right state
without uploading anything:

```bash
curl -s http://<ip>/info      # {"version":"2.2.0","slot":"app0"}
curl -s -o /dev/null -w '%{http_code}\n' http://<ip>/      # 200 = window is open
```

### Reading the result

A successful run looks like this on the serial log. **Watch for the slot
changing** — that is the only proof it was not a silent no-op:

```
[OTA] upload window open — http://192.168.0.113/ (closes in 300s)
[OTA] upload starting: esp32_firmware.ino.bin (1232335 B)
[OTA] written — rebooting
Firmware 2.2.0 on app1 (pending verify)
[OTA] image on app1 marked valid after 30s healthy
```

`(pending verify)` means the image is on probation. If it fails to stay booted
and connected for 30 s it reverts to the previous slot by itself, so a bad image
cannot brick the panel — but it also means **you have not really succeeded until
you see `marked valid`**. Report a flash as done only after that line.

### When it goes wrong

| Symptom | Cause |
|---|---|
| `ERR` immediately after YES | No IP. The window is refused rather than opened on an address that does not exist. |
| Browser cannot reach the URL | Different network. Some routers separate 2.4/5 GHz; the ESP32 is 2.4 GHz only. |
| `[OTA] upload aborted by client` | Connection dropped mid-transfer. The panel reports `ERR` and closes the window; nothing is half-written and the old slot is untouched. Just retry. |
| `[OTA] upload ended without a verdict` | A malformed POST. Same recovery. |
| Upload rejected part way | Wrong file. It must be the `.bin`, not the `.elf` or a merged image. `Update` checks the `0xE9` magic byte and the partition size. |

**Do not go looking for firewall problems.** Earlier revisions of this repo
pulled the image from the build host and that never worked here: the panel is one
router hop away (host→gateway `ttl=64`, host→device `ttl=63`), so its ARP for an
apparently on-link host never crossed. A `ufw allow` was added on that wrong
diagnosis and made no difference. The upload flow runs browser → device, the
direction that works, and needs no rule at all.

## 6. Hardware e2e without an encoder

`test_suite.py <ip> 81` needs the device in **Network Canvas** mode. Do NOT
assume that means physical navigation is required — it is not, and asserting so
without checking wasted a round trip once already.

The WebSocket server's listening socket is opened in `setup()`, so a host can
complete a **TCP connect** to port 81 from any mode. But `webSocket.loop()` is
only pumped inside `NetworkMode::update()`, so from the boot menu the
**WebSocket handshake times out**. A bare TCP probe is therefore not evidence
the device is reachable.

Use the compile-time boot-mode seam instead. It is `#ifdef`-guarded and costs
nothing in a normal build (verified: production builds are byte-for-byte the
same size with and without it):

```bash
# AppMode: 0 MENU, 1 CLOCK_CAL, 2 NETWORK, 3 TIMER, 4 OCTOPUS, 5 GAMES, 6 SETTINGS
arduino-cli compile -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --build-property "compiler.cpp.extra_flags=-DLEDDITE_BOOT_MODE=2" \
  --output-dir build/fw-net esp32_firmware/esp32_firmware.ino
arduino-cli upload -p /dev/ttyUSB0 -b esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --input-dir build/fw-net esp32_firmware/esp32_firmware.ino
.venv/bin/python test_suite.py <ip> 81
```

Look for `[TEST] boot mode forced to 2` in the boot log. Only the mode *entry*
is forced; the protocol handler, canvas, serpentine mapping and ACK path are all
production code.

**Reflash a clean production image when you are done.** A test image re-triggers
its hook on every boot, and the absence of a `[TEST]` line in the banner is how
you prove the image now running is the clean one.

## 7. What has NOT been verified on hardware

Be honest about these rather than reporting them as passing. Both need a person
in front of the panel:

- **The five games rendered on the physical LEDs.** They are covered by 740k
  unit assertions and by contact sheets (`make ux-review`), but nobody has
  watched Snake, Invaders, Dino, Pong and Bricks run on the panel. The sheets
  render the raw buffer and structurally cannot show FastLED's global brightness
  scaling — a screen can look right there and be invisible at level 1.
- **`test_suite.py` against real hardware.** It has only ever run against the
  simulator plus one 6/6 pass in Network Canvas mode; it is not part of any gate.

Verified end to end and safe to state as done: USB flash, the OTA upload flow in
both the success and abort paths, deferred rollback in both directions, and the
boot seams.

## 8. Gotchas that have cost time here

- **`make test` is not the gate.** Run all five checks in the `validate` skill.
  Native tests recompile `src/` and are blind to the committed WASM the browser
  actually loads.
- **`tools/sync-firmware-copies.sh`'s `MODULES` list is the rule.** It once
  omitted `TextRenderer`, so a new glyph never reached the device while
  `--check` reported everything in step. If you add a duplicated file, add it to
  that list.
- **Rebuild and commit the WASM in the same commit as any `src/` change**
  (`RULES.md` §2). `make check-wasm` only proves the committed artifact *has*
  bindings, not that it is current.
- **`gh pr edit` fails on this repo** with a GraphQL "Projects (classic) is being
  deprecated" error. Use `gh api -X PATCH repos/<owner>/<repo>/pulls/<n>`
  instead.
- **Never print `wifi_credentials.h`.** Refer to macro names, never values.
