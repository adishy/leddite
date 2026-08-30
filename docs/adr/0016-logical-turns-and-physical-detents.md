# 0016 — Logical turns and physical detents are different inputs

- **Date:** 2026-08-30
- **Status:** Accepted
- **Context branch:** `v2-games-overhaul`

## Context

The vertical submenus read wrong on the device. Turning the knob one way moved
the selection the opposite way to what the hand expected, and one detent per row
overshot on almost every input — you feel for one row on a five-item list and
land two down.

Both are properties of the *physical* encoder, not of the menu. But
`UiController::turn(delta)` was being handed raw detents by the firmware and
logical rows by the tests, the WASM harness and the simulator's screen-jump
helper. Changing its meaning to fix the feel would have rewritten roughly forty
call sites whose intent was "move one row", and left the API meaning two things
depending on the caller.

## Decision

**Two entry points, with different units.**

| | Units | Callers |
|---|---|---|
| `turn(delta)` | one row / one level / one game | tests, WASM harness, programmatic walks |
| `encoderTurn(detents)` | raw encoder clicks | `UiMode::onEncoderTurn`, the simulator's knob |

`encoderTurn()` applies two transforms, and only on the vertical lists
(`GAMES_MENU`, `SETTINGS_MENU`, `PLACES_MENU`):

- **Inverted.** The list moves, not the cursor — turning the knob brings the row
  below up into the selection, the way a physical dial behind a window does.
- **`MENU_DETENTS_PER_ROW = 2`.** Two clicks per row. Costs a little travel,
  removes the overshoot entirely.

Everywhere else it is `turn()` unchanged: on the brightness editor the number
itself is the thing being turned up, and inverting or halving that would be
wrong in both respects. Skipping games mid-play stays one click per game.

The leftover half-detent is reset on every screen entry and on both gestures, so
a part-turn never steers the next menu on its first click — a bug that is
invisible until someone half-turns out of a menu and the next one jumps.

## Consequences

**Good**

- Zero churn in the existing tests: they kept saying "one row" and still mean it.
- The transform is in `src/`, so it is unit-tested (eight assertions covering
  direction, granularity, accumulation and the reset) and the simulator gets it
  for free through the same binding the firmware uses.
- The simulator's knob does not feel better than the device's. A simulator with
  nicer input than the hardware is one you cannot use to judge feel — the same
  argument as [0010](0010-simulator-runs-the-real-state-machine.md), applied to
  input rather than to rendering.

**Costs**

- Two similarly-named methods, which is a real hazard: calling `turn()` from a
  new input path would silently bypass the feel. The header says which is which
  and why, and `UiMode` is the only firmware caller.
- The granularity is a compile-time constant, not a setting. A settings entry for
  it would cost a screen and an NVS key to adjust something a person tunes once
  and never touches; if it turns out to want tuning, that is the next decision,
  not this one.

## Related

- **0010** — the simulator runs the real state machine; this extends that to input.
- **0009** — why the transform lives in `src/` and not in `UiMode`.
