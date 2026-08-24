# 0011 — Words, not icons, on a 16×16 panel

- **Date:** 2026-08-24
- **Status:** Accepted
- **Supersedes:** the icon layout introduced with `WeatherView` in 0009

## Context

The weather view drew eleven procedurally generated icons — sun, crescent moon,
cloud lobes, rain streaks, a lightning bolt — across rows 0–9, with the
temperature underneath on rows 11–14.

Three things went wrong with that.

**The icons were ambiguous.** At 16×16 an icon gets roughly a 10×10 box. Within
that budget "partly cloudy" and "overcast" differ by a couple of pixels, and
"drizzle" and "rain" differ only in whether the streaks are one pixel long or
two. Distinctions the WMO code set makes clearly became distinctions the panel
could not express. Two of them needed dedicated tuning passes during development
just to be recognisable at all: the crescent moon first rendered as a 1px sliver
that read as noise, and the "partly day" cloud occluded the sun it was supposed
to be partly covering.

**They cost the entire top half of the panel** to convey less than one word.

**The temperature had no degree mark.** `SmallTextRenderer` advances 4px per
character, so `-12*C` is 18px on a 16px row. With icons occupying rows 0–9 there
was nowhere to put the overflow, so the degree was dropped — leaving a bare
number whose meaning depended on the user remembering the unit setting.

## Decision

Drop the icons. Render the condition as **words**, in the large 5×7 font,
scrolling.

- `WeatherView::describe()` maps each WMO code to plain uppercase wording
  (`PARTLY CLOUDY`, `FREEZING RAIN`, `SEVERE THUNDERSTORM`).
- `WeatherView::conditionColor()` supplies one accent per condition. The
  temperature, the rule and the description are all tinted from it, so colour
  still carries the state at a glance — which was the one thing the icons were
  genuinely good at.
- Layout becomes temperature on rows 0–3, a 1px rule on row 5, and the scrolling
  description on rows 8–14.
- The degree mark comes back. Where the full string still will not fit,
  `formatTemp()` sheds the **unit letter** rather than the degree: `-12*` rather
  than `-12C`. The unit is a user setting shown elsewhere in the UI; the degree
  is what makes the number a temperature at all.

A failed fetch reads `NO DATA` in neutral grey. This is a distinct state, not a
fallback to code 0 — which is "clear sky", and would present a dead network as a
sunny afternoon.

## Consequences

**Good**

- Every condition is now unambiguous, including ones the icon set could not
  distinguish.
- `describe()` and `conditionColor()` are pure functions over a `uint8_t`, so
  the whole condition mapping is exhaustively testable against the documented
  WMO set rather than sampled. The tests assert every documented code has
  distinct wording, that no description exceeds the scratch buffer, and that
  every character is one the font actually has — an unrenderable character would
  otherwise appear as a silent gap mid-word.
- ~150 lines of procedural drawing primitives (`disc`, `cloud`, `cloudSmall`,
  `sun`, `moon`) and their tuning constants are gone.

**Costs**

- **The condition is no longer legible instantly.** An icon is apprehended in a
  glance; a scrolling word takes up to a few seconds to read in full. The colour
  accent is the mitigation, not a replacement. For a view that already rotates
  on a 10 s timer this is an acceptable trade, but it is a real regression for
  someone glancing across a room.
- The description needs a 2,772-byte static scratch buffer for the 5×7 render
  (`DESC_MAX_CHARS × 6 × 7 × 3`), against 384 bytes for the small-font buffer.
- The weather face must now redraw every frame to animate the scroll. It already
  did — the old layout animated precipitation — so nothing changed in
  `TimeMode`, but the face is no longer a candidate for static-render
  optimisation.
- Wording is English and uppercase-only, since that is the font's entire
  character set.

## Related

- **0009** — why `WeatherView` is in `src/` and Arduino-free, which is what makes
  the mapping tables testable at all.
- **0010** — the simulator renders this view from the same code, so the wording
  and colours can be checked in a browser without hardware.
