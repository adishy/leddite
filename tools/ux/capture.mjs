// Captures what the real UI state machine renders, as frames, for visual review.
//
// This drives the COMMITTED simulator/leddite_wasm.js through its DeviceUI
// binding — the same `src/UiController` the ESP32 runs (docs/adr/0010). So the
// pixels captured here are the pixels the device produces, not an approximation
// of them.
//
// Output is a JSON array of { scene, label, t, px } where px is 768 bytes of
// RGB. tools/ux/review.py turns that into a contact sheet and a metrics table.
//
//   node tools/ux/capture.mjs <simulator-dir> <out.json>
//
// WHAT THIS CANNOT SEE
// --------------------
// MenuMode (the boot menu), TimeMode (clock/date), TimerMode and OctopusMode
// still live only in esp32_firmware/ and depend on Arduino, so they have no WASM
// binding and cannot be captured. That gap is the unfinished half of
// docs/adr/0009 — mode logic that has not moved to src/ cannot be reviewed
// without hardware.

import { createRequire } from 'module';
import { readFileSync, writeFileSync } from 'fs';
import { resolve } from 'path';

const require = createRequire(import.meta.url);
const [simDirArg, outPath] = process.argv.slice(2);

if (!simDirArg || !outPath) {
  console.error('usage: node tools/ux/capture.mjs <simulator-dir> <out.json>');
  process.exit(2);
}

// require() treats a bare relative path as a module name, not a file, so the
// simulator directory has to be absolute before it is handed over.
const simDir = resolve(simDirArg);

const factory = require(`${simDir}/leddite_wasm.js`);
const mod = await factory({ wasmBinary: readFileSync(`${simDir}/leddite_wasm.wasm`) });

const shots = [];
const grab = (ui) => Array.from(new Uint8Array(mod.HEAPU8.buffer, ui.getBuffer(), 768));

// `strip` marks frames that belong to one animation, so review.py lays them out
// as a filmstrip in time order rather than as unrelated thumbnails.
function shot(scene, label, t, ui, strip = null) {
  shots.push({ scene, label, t, strip, px: grab(ui) });
}

function withUI(fn) {
  const ui = new mod.DeviceUI();
  try { fn(ui); } finally { ui.delete(); }
}

// Advances to `toMs` at roughly the firmware's frame rate.
//
// GameEngine::update() is rate-limited: it steps at most once per call, however
// far the clock jumped. Ticking straight from 0 to 9000 therefore advances the
// game by one step, not by nine seconds of play, and captures a frame that looks
// nothing like what the device shows. Anything stateful must be walked forward.
// (Scroll offsets are pure functions of elapsed time and do not need this.)
const FRAME_MS = 33;
function run(ui, fromMs, toMs) {
  for (let t = fromMs; t <= toMs; t += FRAME_MS) ui.tick(t);
  return toMs;
}

// ── Menus ─────────────────────────────────────────────────────────────────────
// Every item of every list, so band contrast can be judged against each accent
// colour rather than against whichever one happens to be selected first.

withUI((ui) => {
  const NAMES = ['SNAKE', 'LIFE', 'INVADERS', 'DINO', 'CYCLE ALL'];
  ui.enterGames(0);
  for (let i = 0; i < NAMES.length; i++) {
    ui.tick(0);
    shot('menu.games', `games: ${NAMES[i]}`, 0, ui);
    ui.turn(1, 0);
  }
});

withUI((ui) => {
  const NAMES = ['BRIGHTNESS', 'PLACE', 'UNITS'];
  ui.enterSettings(0);
  for (let i = 0; i < NAMES.length; i++) {
    ui.tick(0);
    shot('menu.settings', `settings: ${NAMES[i]}`, 0, ui);
    ui.turn(1, 0);
  }
});

withUI((ui) => {
  ui.enterSettings(0);
  ui.turn(1, 0);       // PLACE
  ui.press(0);         // open places menu
  for (let i = 0; i < 7; i++) {
    ui.tick(0);
    shot('menu.places', `place ${i}`, 0, ui);
    ui.turn(1, 0);
  }
});

// A long label scrolling: the dwell must hold long enough to read the opening,
// and the wrap seam must never leave the row blank.
withUI((ui) => {
  ui.enterGames(0);
  ui.turn(4, 0);       // CYCLE ALL — 33px, the widest label
  for (const t of [0, 600, 1200, 1800, 2400, 3000, 3600, 4200]) {
    ui.tick(t);
    shot('menu.scroll', `CYCLE ALL t=${t}ms`, t, ui, 'menu.scroll');
  }
});

// ── Editors ───────────────────────────────────────────────────────────────────

withUI((ui) => {
  ui.enterSettings(0);
  ui.press(0);         // brightness editor
  for (let lv = 1; lv <= 10; lv++) {
    ui.setBrightnessLevel(lv);
    ui.tick(0);
    shot('edit.brightness', `brightness ${lv}`, 0, ui);
  }
});

withUI((ui) => {
  ui.enterSettings(0);
  ui.turn(2, 0);       // UNITS
  ui.press(0);
  for (const u of [0, 1]) {
    ui.setUnit(u);
    ui.tick(0);
    shot('edit.units', `units ${u ? 'F' : 'C'}`, 0, ui);
  }
});

// ── Weather ───────────────────────────────────────────────────────────────────
// One sample per condition just after the place flash clears, so the wording,
// the rule colour and the temperature can all be read at once.

const CONDITIONS = [
  [0, 1, 'CLEAR SKY'], [0, 0, 'CLEAR NIGHT'], [1, 1, 'MAINLY CLEAR'],
  [2, 1, 'PARTLY CLOUDY'], [3, 1, 'OVERCAST'], [45, 1, 'FOG'],
  [51, 1, 'LIGHT DRIZZLE'], [63, 1, 'RAIN'], [65, 1, 'HEAVY RAIN'],
  [66, 1, 'FREEZING RAIN'], [73, 1, 'SNOW'], [77, 1, 'SNOW GRAINS'],
  [81, 1, 'RAIN SHOWERS'], [95, 1, 'THUNDERSTORM'], [99, 1, 'SEVERE THUNDER'],
];

const STEADY = 2100;   // just past WeatherView::PLACE_FLASH_MS

for (const [code, day, label] of CONDITIONS) {
  withUI((ui) => {
    ui.setWeather(185, code, !!day, true);
    ui.enterWeather(0);
    ui.tick(STEADY);
    shot('weather.conditions', label, STEADY, ui);
  });
}

// Temperature edge cases — the widths that forced the unit letter to be shed.
const TEMPS = [
  [185, 0, 'C', '18C'], [-125, 0, 'C', '-13C'], [-405, 0, 'C', '-40C'],
  [185, 1, 'F', '65F'], [1000, 1, 'F', '212F'], [-405, 1, 'F', '-40F'],
];
for (const [c10, unit, u, label] of TEMPS) {
  withUI((ui) => {
    ui.setWeather(c10, 3, true, true);
    ui.setUnit(unit);
    ui.enterWeather(0);
    ui.tick(STEADY);
    shot('weather.temps', `${label}`, STEADY, ui);
  });
}

// The place flash and the failure state — both are things a user will see.
withUI((ui) => {
  ui.setWeather(185, 0, true, true);
  ui.enterWeather(0);
  ui.tick(300);
  shot('weather.states', 'place flash', 300, ui);
});
withUI((ui) => {
  ui.setWeather(0, 0, true, false);
  ui.enterWeather(0);
  ui.tick(STEADY);
  shot('weather.states', 'fetch failed', STEADY, ui);
});

// A full scroll cycle of the longest description, sampled across the window
// TimeMode actually gives this view. If the tail never appears, the wording is
// unreadable in practice however good it looks in a still.
withUI((ui) => {
  ui.setWeather(185, 99, true, true);
  ui.enterWeather(0);
  for (let t = 2000; t <= 10000; t += 800) {
    ui.tick(t);
    shot('weather.scroll', `SEVERE THUNDERSTORM t=${t}ms`, t, ui, 'weather.scroll');
  }
});

// ── Games ─────────────────────────────────────────────────────────────────────
// Sampled well into play, not at frame zero: a game that looks fine on its
// opening frame can still be visually dead a few seconds later.

const GAMES = ['SNAKE', 'LIFE', 'INVADERS', 'DINO'];
for (let g = 0; g < GAMES.length; g++) {
  withUI((ui) => {
    ui.enterGames(0);
    ui.turn(g, 0);
    ui.press(0);
    let now = 0;
    for (const t of [1000, 4000, 9000]) {
      now = run(ui, now, t);
      shot('games', `${GAMES[g]} t=${t / 1000}s`, t, ui);
    }
  });
}

writeFileSync(outPath, JSON.stringify(shots));
console.log(`captured ${shots.length} frames -> ${outPath}`);
