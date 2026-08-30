#!/usr/bin/env bash
# Copies the Arduino-free core modules from src/ + include/ into esp32_firmware/.
#
# The Arduino IDE compiles every .cpp/.h sitting beside the .ino, and cannot see
# ../src or ../include, which is why these files exist twice. See RULES.md §2 and
# docs/adr/0009. This script makes the duplication mechanical and checkable
# instead of hand-maintained; RULES.md §2 is satisfied by running it.
#
#   tools/sync-firmware-copies.sh          # copy
#   tools/sync-firmware-copies.sh --check  # fail if any copy is stale
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

# Both halves of the duplicated core, exactly as CLAUDE.md lists them.
#
# The protocol set was missing here until a '/' glyph added to src/TextRenderer
# never reached the device: the firmware kept a font with a blank where the
# slashes in "HTTP://192.168.0.113" should be, and --check reported everything
# in step because it was not looking. A guard that covers less than the rule it
# enforces is worse than no guard, because it is believed.
MODULES=(Canvas Transformer MarqueeEngine ProtocolHandler TextRenderer
         ColorUtils Draw SmallTextRenderer ListMenu GameEngine
         BrightnessModel WeatherView Places UiController)

CHECK=false
[ "${1:-}" = "--check" ] && CHECK=true

STALE=()
for m in "${MODULES[@]}"; do
  for pair in "include/$m.h:esp32_firmware/$m.h" "src/$m.cpp:esp32_firmware/$m.cpp"; do
    src="${pair%%:*}"; dst="${pair##*:}"
    [ -f "$src" ] || continue
    if $CHECK; then
      if ! cmp -s "$src" "$dst" 2>/dev/null; then STALE+=("$dst"); fi
    else
      cp "$src" "$dst"
    fi
  done
done

if $CHECK; then
  if [ "${#STALE[@]}" -gt 0 ]; then
    echo "ERROR: esp32_firmware/ copies are stale (RULES.md section 2):" >&2
    printf '  %s\n' "${STALE[@]}" >&2
    echo "" >&2
    echo "Run: tools/sync-firmware-copies.sh" >&2
    exit 1
  fi
  echo "sync-firmware-copies: all esp32_firmware/ copies match src/"
else
  echo "sync-firmware-copies: copied ${#MODULES[@]} modules into esp32_firmware/"
fi
