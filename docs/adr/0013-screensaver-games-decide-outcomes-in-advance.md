# 0013 — Screensaver games decide their outcomes in advance

- **Date:** 2026-08-30
- **Status:** Accepted
- **Context branch:** `v2-games-overhaul`
- **Builds on:** 0009 (mode logic in `src/`), 0012 (unlit backgrounds)

## Context

The game screensavers had two complaints against them, and they turned out to be
the same complaint.

**Dino ran through cacti.** The dino would clear an obstacle, come down on top of
it, and keep running — the sprite visibly intersecting the scenery. Reading the
code, there was no collision detection at all. Takeoff was triggered by a
hand-derived window:

```cpp
const int16_t gap = obsX[i] - 45;
if (gap > 2 * dinoSpeed && gap <= 5 * dinoSpeed) dinoVY = JUMP_V0;
```

That window was fitted to a 3px-wide sprite and a nine-step arc whose useful
clearance lasted four steps. At the slow end of the speed ramp a cactus needed
seven steps to cross the dino. The arithmetic never worked; nothing checked it,
so nothing said so.

**Invaders was asked for a 0.3 win rate.** The obvious reading is to tune the
cannon's accuracy and the invaders' fire density until roughly three runs in ten
end in a win, then leave the constants alone. That is a rate which is a
*consequence* of about a dozen numbers — formation sizes, march cadence, fire
percentages, boss HP, bullet speed — and it moves whenever any of them is
touched. It also cannot be asserted on cheaply: proving a tuning still yields 0.3
means running the simulation, which means the test is measuring the very thing it
is supposed to be protecting.

Both are the same shape: a property that matters visually was left to emerge
from parameters, and emergent properties in a system nobody is watching drift
until they look broken.

## Decision

**Where a game's outcome is a requirement, compute or draw it explicitly rather
than tuning parameters until it appears.**

### Dino plans the arc it is about to fly

`dinoArcSafe(delay)` simulates the full 15-step jump against the actual obstacle
positions and reports whether every step of it clears. Takeoff is:

```cpp
if (grounded && dinoCrashComing())
    if (dinoArcSafe(0) && !dinoArcSafe(1)) dinoVY = JUMP_V0;
```

— jump on the *last* step from which the flight is still clean. `dinoWouldHit()`
is shared between the planner and the real per-step collision check, so a plan
cannot disagree with its own outcome. Roughly 90 predicate evaluations per
grounded step, which is nothing at a 60ms tick.

Two supporting constraints make the plan sound rather than merely plausible:

- **The scroll speed only changes on an empty track.** The planner assumes a
  constant speed for the whole flight; a ramp tick partway through an approach
  silently invalidates a plan that was correct when it was made. This alone was
  worth about one crash per 22,000 steps, and every crash the diagnostic caught
  landed on the step the speed changed.
- **Obstacles only spawn while the dino is grounded**, and at least a flight's
  length behind the last one. An obstacle appearing mid-flight is an input the
  committed plan never saw.

Collision detection still runs every step. It is expected never to fire —
`test_dino_never_hits_an_obstacle` asserts `dinoCrashes() == 0` across 800,000
steps — and exists so that a future planning failure reads as a stumble rather
than as a dino phasing through solid scenery.

### Invaders draws its verdict once, then plays the campaign for real

At campaign start the engine rolls `heroRun = rndN(100) < winChancePct`. A hero
run dodges every shot to the end of the campaign. Otherwise a doom level is drawn
from the middle of the campaign; on that level the ship stops dodging while the
invaders start aiming at its column, and it dies under visibly converging fire.
Clearing the doom level does not save the run.

Everything else is played out: which invaders die when, how each boss fight goes,
how long the run lasts, how many waves recycle. Only the verdict is pre-drawn.
The dodge is rendered as a sidestep with the shot bursting on the deck beside the
ship, so "the ship survives" is a thing you watch happen rather than a bullet
passing through a hull.

`setInvadersWinChance()` makes it a dial, and two tests hold it: one asserts the
measured rate sits in [0.27, 0.33] over ~10,000 runs, the other that 0% yields no
wins and 100% yields nothing else.

### A formation reaching the deck is not a loss

It recycles to the top as a fresh wave. Making it fatal would put a hard time
limit on every level, and that limit would have to be re-derived against the
cannon's clear rate every time a formation size changed. Death comes from enemy
fire and nothing else — one dial instead of two.

## Also in this change

- **Game of Life is removed.** Requested, and it was the one game with no agent
  in it to reason about. It also carried five 1024-byte world buffers; the
  firmware's global footprint dropped from 75,948 to 71,004 bytes.
- **Pong and Breakout added.** Both keep sub-pixel state (`/16` px): on a 16px
  field, integer velocities can only express 45° diagonals and a rally looks like
  a metronome. Both carry hue-cycling balls with trails, which is the whole
  reason they earn a slot next to Snake.
- **Breakout has an anti-stall timer.** A deterministic paddle plus a
  deterministic ball settles into a limit cycle: with one brick left in a top
  corner the ball orbited the right of the field for 27 simulated minutes without
  reaching it. The paddle's aim error is now redrawn on every return, and a
  300-step idle timer re-serves, then concedes the field.
- **The T-Rex is a 7×6 sprite.** A head block high on the right, a tail tip held
  clear of the back at the far left, and two separated legs. Dropping any one of
  the three turns it back into a blob, so all three are asserted in
  `test_dino_sprite_reads_as_a_dinosaur` rather than left to a screenshot review.

## Consequences

**Good**

- The dino cannot hit a cactus, and the reason is a checkable invariant rather
  than a fitted constant.
- The Invaders win rate is exactly `winChancePct` regardless of how the levels
  are subsequently retuned. It was 0.315 against a 0.30 dial before this.
- Every game's headline property now has a test that fails when it breaks.

**Costs**

- **The Invaders outcome is scripted, and that is a real thing to give up.** A
  viewer who watched closely for long enough would find that the ship's survival
  does not depend on the shots on screen. The alternative was a rate that drifts
  silently, which is worse for a device nobody is watching — but this is a
  trade, not a free win.
- The dino's takeoff planner constrains obstacle *design*: shapes are paired so
  the widest is also the lowest, because clearance time falls as a cactus grows
  and rises as it widens. Adding a tall, wide obstacle would need the arc
  retuned, and the soak test is what would tell you.
- Obstacles are sparse — one on screen at a time — because spawn separation is
  what makes the planner's single-obstacle reasoning sound.

## Related

- **0009** — why this is all in `src/` with time and randomness injected. None of
  these soak tests could exist otherwise.
- **0012** — the background rule the two new games are written against, and the
  test that enforces it across `Game::COUNT`.
