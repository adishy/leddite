# 0012 — Unlit is the best background this panel has

- **Date:** 2026-08-24
- **Status:** Accepted
- **Amends:** the selected-row treatment introduced alongside 0011

## Context

Three parts of the UI had, independently, arrived at the same idea: fill an area
with a colour and draw the content on top of it.

- `ListMenu` drew the selected row as black text knocked out of a filled accent
  band.
- `WeatherView` drew a 1px rule between the temperature and the description.
- `GameEngine`'s Dino filled the panel with a pale "day" sky (20, 24, 30) and
  drew a dark grey dino (60, 70, 80) and ground on top of it.

On a monitor all three look fine. On the actual hardware they do not, and the
reason is a property of the display rather than a matter of taste.

A WS2812B panel has no reflective surface and no black level to speak of — an
unlit pixel is *off*, which is the deepest black the device can produce and an
enormous contrast step from anything lit. Two *lit* colours, by comparison, are
separated only by their difference in intensity and hue, through a diffuser, at
whatever global brightness the user has chosen. The Dino case was the extreme:
sky and sprite were about 3× apart in luma, and on the panel the dino was
genuinely hard to pick out of its own background.

The knocked-out menu band had a second problem on top of that. Reverse text is
harder to read than positive text at any size, because the letterforms become
the gaps rather than the marks; at a 3×4 glyph there is not enough resolution to
absorb that penalty. A 7px full-width band is also 112 lit pixels at full accent
brightness — a glare block that dominates the panel and, at high brightness
levels, a meaningful share of the power budget for no information.

## Decision

**Treat unlit pixels as the default background everywhere. Reserve filled areas
for things that are themselves the content.**

Concretely:

- `ListMenu`'s selected row is drawn in the **5×7 medium font** at full accent
  brightness on black; unselected rows stay in the 3×4 small font, dimmed.
  Selection is signalled by **size and brightness**, not by a background.
  Rows are consequently no longer a fixed height — 7 + 4 + 4 = 15 fills the
  panel above the position bar exactly, which is also why the inter-row gutters
  are gone. The size difference separates the rows on its own.
- `WeatherView` drops the divider rule. Rows 4–7 are simply blank.
- Dino draws on black. Day and night now differ in the palette of what is
  *drawn* — warm and bright by day, cool and dimmer by night — not in what is
  behind it.
- `Draw::stencilClipped()`, added to make black-on-band text possible at all, is
  removed. It has no remaining caller, and keeping a primitive whose only
  purpose is a rejected technique invites its reuse.

Invaders keeps its `(0, 0, 6)` deep-space tint. At a peak channel of 6 it is
effectively off, and it survives the rule below deliberately.

## The rule, as a test

`test_no_game_has_a_bright_background` finds the most common colour in each
frame; if it covers more than half the panel it is the background, and its peak
channel must be ≤ 12. This generalises the Dino fix to every game written from
now on rather than fixing one instance.

It was verified to fail against the old Dino code before being kept — a test
that cannot fail is worth nothing.

`ListMenu` gained the matching assertion in the other direction: no row may be
lit edge to edge, which is exactly what a regression to a filled band produces.

## How the decision was made

Both menu treatments were built and rendered through `make ux-review`, which
drives the real state machine and produces contact sheets. Put side by side, the
banded version was visibly the harder of the two to read at a glance, before any
argument about hardware. The hardware reasoning explains *why*; the images are
what settled it.

This is the first decision in this repo made from rendered output rather than
from reasoning about the code, which is the whole point of that skill existing.

## Consequences

**Good**

- Selection is unmistakable and the label is materially easier to read.
- Less of the panel is lit, which matters directly at brightness levels 8–10
  where an all-white frame already exceeds the supply budget (see 0009).
- The principle is now enforced by tests in both `GameEngine` and `ListMenu`
  instead of living in someone's memory.

**Costs**

- **Only about two characters of the selected label are on screen at once.**
  `TextRenderer`'s stride is a fixed 6px, so nearly every menu label now scrolls
  where before four small-font characters fitted without moving. Partial glyphs
  at the row edges are accepted; the label is read over time rather than at a
  glance. For a menu the user is actively turning through, that is the right
  trade — but it is a real loss of at-a-glance scanning.
- Row heights are no longer uniform, so anything reasoning about row geometry
  must go through `rowTop()` / `rowHeight()` rather than multiplying by a
  constant.
- With gutters gone, the clip is the only thing keeping a 7px glyph out of its
  neighbour's row. A test covers it.

## Related

- **0011** — the weather view's move to words, which this amends by removing its
  divider rule.
- **0010** — the simulator running the real state machine, without which the
  side-by-side comparison that decided this would not have been possible.
