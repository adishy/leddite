---
name: ux-review
description: Visually review the Leddite UI by running the real firmware state machine in the simulator, rendering every screen to contact sheets, and grading legibility and usability. Use after changing anything that draws — ListMenu, WeatherView, BrightnessModel, GameEngine, Draw — and before flashing a UI change to hardware.
---

# Leddite visual UX review

`make test` proves the UI is *correct*. It cannot tell you the UI is *usable*.
Every bug this repo has shipped to the panel was of the second kind: a crescent
moon that rendered as a 1px sliver, a cloud that occluded the sun it was
partially covering, a menu band so dim the label on it disappeared. All of them
passed their tests.

This skill closes that gap by rendering what the device actually draws and
requiring you to look at it.

```bash
make ux-review          # capture + sheets + metrics -> build/ux/
```

Then **open every PNG it lists with the Read tool.** The metrics table is the
smaller half of this skill; skipping the images defeats the point.

## Rebuild first if you touched the C++

```bash
make simulator          # only if src/, include/ or wasm_bridge.cpp changed
```

`capture.mjs` drives the **committed** `simulator/leddite_wasm.js`, so a stale
artifact means you are reviewing the previous version of the UI and will not be
able to tell. See the `rebuild-wasm` skill.

## What gets captured

`tools/ux/capture.mjs` drives `src/UiController` through its `DeviceUI` binding —
the same state machine the ESP32 runs (`docs/adr/0010`), so these are the
device's pixels, not a mock-up.

| Scene | Covers |
|-------|--------|
| `menu.games` / `menu.settings` / `menu.places` | every item of every list, so band contrast is judged against each accent, not just the first |
| `menu.scroll` | a long label through dwell → scroll → wrap |
| `edit.brightness` | all ten levels |
| `edit.units` | C and F |
| `weather.conditions` | fifteen WMO conditions |
| `weather.temps` | the widths that force the unit letter to be shed |
| `weather.states` | place flash, and a failed fetch |
| `weather.scroll` | a full pass of the longest description across the 10 s the view is on screen |
| `games` | each game at 1 s, 4 s and 9 s of play |

Animated scenes render as a **filmstrip** in time order; the rest as a grid.

## Grade against these

Judge each screen and say plainly which fail. A finding with no fix is still
worth reporting — record it rather than quietly accepting it.

1. **Legibility.** Can you read the text without knowing what it says? Reverse
   text (black knocked out of a band) is harder to read at 3×4 than positive
   text, because the letterforms become the gaps.
2. **Distinguishability.** Do two different states look the same? Check the
   *opening* frame of scrolling text especially — several weather conditions
   share a first word and are told apart only by colour until the scroll
   advances.
3. **Timing.** Does scrolling text complete a pass before its screen is replaced?
   `weather.scroll` covers exactly the window `TimeMode` allows.
4. **Layout.** Do the reserved gap rows stay dark? The metrics table flags this,
   but check the image for crowding it cannot see.
5. **Colour.** Does the accent match its meaning — warm for sun, blue for rain,
   white for snow? Would it survive the panel's real brightness range?
6. **Failure states.** A failed fetch must be visibly distinct from a real
   reading. `weather.states` has both side by side.

## Reading the metrics table

- `fill` — lit fraction. Sparse is fine for games (Snake is a handful of pixels);
  a frame under 1.5% is flagged as **NEARLY BLANK**.
- `colors` — distinct lit colours. A drop to 1 on a screen that should be
  multicoloured usually means a palette collapsed.
- `peak` — brightest ground on a row that has both ground and knocked-out text.
  Under 60 is flagged: black text will not read on it. `-` means no band was
  present, which is expected for a full-bleed game frame, not a failure.
- `animation: N/M consecutive frames differ, dwell Xms` — a leading run of
  identical frames is the deliberate dwell, not a stall. Only **NEVER MOVES**
  is a defect.

## What this cannot see

`MenuMode` (the boot menu), `TimeMode` (clock and date), `TimerMode` and
`OctopusMode` still live only in `esp32_firmware/` and depend on Arduino, so
they have no WASM binding and cannot be captured at all. Reviewing those needs
hardware and eyes on the panel.

That gap is the unfinished half of `docs/adr/0009`: mode logic that has not moved
to `src/` cannot be tested *or* reviewed. If you are asked to review one of those
screens, say so rather than reviewing the screens you happen to be able to reach.

## Requirements

Node (for the WASM harness) and Pillow, which is in `requirements.txt`:

```bash
uv pip install -r requirements.txt
```

Works on Linux and macOS; nothing here touches a serial port or the device.
