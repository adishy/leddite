// Drives the COMMITTED leddite_wasm.js through the DeviceUI binding.
//
// Why this exists alongside the native unit tests: those compile src/ afresh and
// so cannot see problems in the artifact the browser actually loads. Both times
// the simulator went black (see the WASM pitfalls in CLAUDE.md) the cause was
// exactly that — a .wasm built without wasm_bridge.cpp, and a missing `Module`
// pre-declaration. Native tests passed throughout. This harness catches that
// class of regression by loading the real file and calling the real bindings.
//
// Run: make test-wasm

import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { readFileSync } from 'fs';

const require = createRequire(import.meta.url);
const here = dirname(fileURLToPath(import.meta.url));

// Mirrors UiController::Screen
const S = {
  GAMES_MENU: 0, GAME_PLAYING: 1, SETTINGS_MENU: 2,
  BRIGHTNESS_EDIT: 3, PLACES_MENU: 4, UNITS_EDIT: 5, WEATHER: 6,
};
const GAME = { SNAKE: 0, LIFE: 1, INVADERS: 2, DINO: 3 };

const WEATHER_STEADY = 5000;   // past WeatherView::PLACE_FLASH_MS

let passed = 0, failed = 0;
const fail = (msg) => { console.error(`    FAIL: ${msg}`); failed++; };
const ok = () => { passed++; };

function check(cond, msg) { cond ? ok() : fail(msg); }
function eq(a, b, msg) { a === b ? ok() : fail(`${msg} — got ${a}, want ${b}`); }

function test(name, fn) {
  process.stdout.write(`  ${name.padEnd(48)} `);
  const before = failed;
  try { fn(); } catch (e) { fail(`threw ${e && e.message ? e.message : e}`); }
  console.log(failed === before ? 'OK' : '');
}

// ── Frame helpers ─────────────────────────────────────────────────────────────

function frame(mod, ui) {
  const ptr = ui.getBuffer();
  return new Uint8Array(mod.HEAPU8.buffer, ptr, 16 * 16 * 3).slice();
}
const litCount = (f) => {
  let n = 0;
  for (let i = 0; i < 256; i++) if (f[i * 3] | f[i * 3 + 1] | f[i * 3 + 2]) n++;
  return n;
};
const anyLit = (f) => litCount(f) > 0;
const px = (f, x, y) => {
  const i = (y * 16 + x) * 3;
  return [f[i], f[i + 1], f[i + 2]];
};
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

// ── Suite ─────────────────────────────────────────────────────────────────────

const createLedditeModule = require(join(here, 'leddite_wasm.js'));

// Hand the glue the .wasm bytes directly. Emscripten's loader otherwise reaches
// for fetch() — which exists in modern node but cannot take a filesystem path —
// and dies with ERR_INVALID_URL. The browser path is unaffected.
const wasmBinary = readFileSync(join(here, 'leddite_wasm.wasm'));
const mod = await createLedditeModule({ wasmBinary });

console.log('=== DeviceUI WASM Harness ===');

test('bindings are present in the committed artifact', () => {
  check(typeof mod.DeviceUI === 'function', 'Module.DeviceUI missing — was wasm_bridge.cpp compiled in?');
  check(typeof mod.Canvas === 'function', 'Module.Canvas missing');
});

test('games menu renders and wraps', () => {
  const ui = new mod.DeviceUI();
  ui.enterGames(0);
  eq(ui.screen(), S.GAMES_MENU, 'screen after enterGames');
  ui.tick(0);
  check(anyLit(frame(mod, ui)), 'games menu rendered blank');

  const first = frame(mod, ui);
  ui.turn(1, 0); ui.tick(0);
  check(!same(first, frame(mod, ui)), 'turning did not change the frame');

  // 5 entries: turning 5 times returns to the start.
  ui.turn(1, 0); ui.turn(1, 0); ui.turn(1, 0); ui.turn(1, 0); ui.tick(0);
  check(same(first, frame(mod, ui)), 'menu did not wrap after a full cycle');
  ui.delete();
});

test('press launches a game, long-press returns to the list', () => {
  const ui = new mod.DeviceUI();
  ui.enterGames(0);
  ui.press(0);
  eq(ui.screen(), S.GAME_PLAYING, 'screen after press');
  eq(ui.currentGame(), GAME.SNAKE, 'first entry should be SNAKE');

  const back = ui.longPress(1000);
  eq(back, false, 'long-press from a game must NOT exit to the main menu');
  eq(ui.screen(), S.GAMES_MENU, 'should be back at the game list');

  // A second long-press is now at a root screen and should exit.
  eq(ui.longPress(2000), true, 'long-press at the game list should exit to main menu');
  ui.delete();
});

test('every game renders live pixels through the binding', () => {
  const ui = new mod.DeviceUI();
  for (let g = 0; g < 4; g++) {
    ui.enterGames(0);
    for (let i = 0; i < g; i++) ui.turn(1, 0);
    ui.press(0);
    eq(ui.currentGame(), g, `selected game index ${g}`);
    let t = 0;
    for (let i = 0; i < 200; i++) { t += 60; ui.tick(t); }
    check(anyLit(frame(mod, ui)), `game ${g} rendered blank`);
  }
  ui.delete();
});

test('CYCLE ALL advances through every game', () => {
  const ui = new mod.DeviceUI();
  ui.enterGames(0);
  for (let i = 0; i < 4; i++) ui.turn(1, 0);      // land on CYCLE ALL
  ui.press(0);
  check(ui.cycling(), 'cycling flag not set');

  const seen = new Set([ui.currentGame()]);
  let t = 0;
  for (let i = 0; i < 4000; i++) { t += 60; ui.tick(t); seen.add(ui.currentGame()); }
  eq(seen.size, 4, 'cycle did not visit all four games');
  ui.delete();
});

test('settings tree navigates and persists values', () => {
  const ui = new mod.DeviceUI();
  ui.enterSettings(0);
  eq(ui.screen(), S.SETTINGS_MENU, 'entered settings');

  // BRIGHTNESS is first.
  ui.press(0);
  eq(ui.screen(), S.BRIGHTNESS_EDIT, 'entered brightness editor');
  const start = ui.brightnessLevel();
  ui.turn(1, 0);
  eq(ui.brightnessLevel(), start + 1, 'brightness did not increase');
  ui.press(0);
  eq(ui.screen(), S.SETTINGS_MENU, 'press should confirm and go back');
  eq(ui.brightnessLevel(), start + 1, 'brightness not retained on exit');
  ui.delete();
});

test('brightness clamps at both ends', () => {
  const ui = new mod.DeviceUI();
  ui.enterSettings(0); ui.press(0);
  for (let i = 0; i < 30; i++) ui.turn(1, 0);
  eq(ui.brightnessLevel(), 10, 'should clamp at 10');
  for (let i = 0; i < 30; i++) ui.turn(-1, 0);
  eq(ui.brightnessLevel(), 1, 'should clamp at 1');
  ui.delete();
});

test('place selection updates the persisted index', () => {
  const ui = new mod.DeviceUI();
  ui.enterSettings(0);
  ui.turn(1, 0);                                  // PLACE
  ui.press(0);
  eq(ui.screen(), S.PLACES_MENU, 'entered places list');
  const before = ui.placeIndex();
  ui.turn(1, 0);
  ui.press(0);
  eq(ui.screen(), S.SETTINGS_MENU, 'back at settings');
  check(ui.placeIndex() !== before, 'place index did not change');
  ui.delete();
});

test('units toggle between C and F', () => {
  const ui = new mod.DeviceUI();
  ui.enterSettings(0);
  ui.turn(1, 0); ui.turn(1, 0);                   // UNITS
  ui.press(0);
  eq(ui.screen(), S.UNITS_EDIT, 'entered units editor');
  eq(ui.unit(), 0, 'default should be Celsius');
  ui.turn(1, 0);
  eq(ui.unit(), 1, 'should toggle to Fahrenheit');
  ui.turn(1, 0);
  eq(ui.unit(), 0, 'should toggle back to Celsius');
  ui.delete();
});

test('weather renders and honours the unit setting', () => {
  const ui = new mod.DeviceUI();
  ui.setWeather(123, 0, true, true);              // 12.3C, clear, day
  ui.enterWeather(0);
  eq(ui.screen(), S.WEATHER, 'entered weather');

  ui.tick(WEATHER_STEADY);
  const celsius = frame(mod, ui);
  check(anyLit(celsius), 'weather rendered blank');

  ui.setUnit(1);
  ui.tick(WEATHER_STEADY);
  check(!same(celsius, frame(mod, ui)), 'switching to Fahrenheit changed nothing');
  ui.delete();
});

test('the weather description scrolls in the committed artifact', () => {
  // The description is the only thing on rows 8-14, so a static frame there
  // means the scroll never started — which is how the view would silently
  // degrade to showing only the first two words.
  const ui = new mod.DeviceUI();
  ui.setWeather(180, 2, true, true);              // 18.0C, PARTLY CLOUDY
  ui.enterWeather(0);

  const descRows = (f) => {
    const out = [];
    for (let y = 8; y <= 14; y++)
      for (let x = 0; x < 16; x++) out.push(...px(f, x, y));
    return out;
  };

  ui.tick(WEATHER_STEADY);
  const early = descRows(frame(mod, ui));
  ui.tick(WEATHER_STEADY + 4000);
  const later = descRows(frame(mod, ui));

  check(early.some((v) => v !== 0), 'description row is blank');
  check(!same(Uint8Array.from(early), Uint8Array.from(later)),
        'description never scrolled');
  ui.delete();
});

test('the temperature row stays clear of the description', () => {
  const ui = new mod.DeviceUI();
  ui.setWeather(-125, 63, true, true);            // -12.5C, worst-case width
  ui.enterWeather(0);
  ui.tick(WEATHER_STEADY);
  const f = frame(mod, ui);

  // Rows 4, 6, 7 and 15 are structural gaps in the layout.
  for (const y of [4, 6, 7, 15]) {
    let lit = false;
    for (let x = 0; x < 16; x++) {
      const [r, g, b] = px(f, x, y);
      if (r | g | b) { lit = true; break; }
    }
    check(!lit, `row ${y} should be a gap`);
  }
  ui.delete();
});

test('the selected menu row knocks its label out in black', () => {
  // Draw::blit treats black as transparent; this only works through
  // Draw::stencilClipped. A regression makes the band a solid bar with no
  // readable label, which litCount alone would not notice.
  const ui = new mod.DeviceUI();
  ui.enterGames(0);
  ui.tick(0);
  const f = frame(mod, ui);

  let black = 0, lit = 0;
  for (let x = 0; x < 16; x++)
    for (let y = 0; y < 4; y++) {
      const [r, g, b] = px(f, x, y);
      (r | g | b) ? lit++ : black++;
    }
  check(lit > 0, 'selected row has no band');
  check(black > 0, 'selected row has no black glyph pixels');
  ui.delete();
});

test('an invalid reading does not render as a valid one', () => {
  const a = new mod.DeviceUI();
  a.setWeather(123, 0, true, true);
  a.enterWeather(0); a.tick(WEATHER_STEADY);
  const good = frame(mod, a);

  const b = new mod.DeviceUI();
  b.setWeather(123, 0, true, false);              // fetch failed
  b.enterWeather(0); b.tick(WEATHER_STEADY);
  const bad = frame(mod, b);

  check(!same(good, bad), 'failed fetch rendered identically to a live reading');
  check(anyLit(bad), 'failure state rendered blank');
  a.delete(); b.delete();
});

test('long-press from settings and weather exits to the main menu', () => {
  const ui = new mod.DeviceUI();
  ui.enterSettings(0);
  eq(ui.longPress(0), true, 'settings root should exit');
  ui.enterWeather(0);
  eq(ui.longPress(0), true, 'weather should exit');

  // But an editor drops back one level first.
  ui.enterSettings(0); ui.press(0);
  eq(ui.longPress(0), false, 'brightness editor should not exit outright');
  eq(ui.screen(), S.SETTINGS_MENU, 'should land back on the settings list');
  ui.delete();
});

test('menus keep the bottom position bar lit', () => {
  const ui = new mod.DeviceUI();
  ui.enterGames(0); ui.tick(0);
  const f = frame(mod, ui);
  let barLit = 0;
  for (let x = 0; x < 16; x++) { const [r, g, b] = px(f, x, 15); if (r | g | b) barLit++; }
  eq(barLit, 16, 'position bar row should be fully drawn (track + thumb)');
  ui.delete();
});

console.log(`\nDeviceUI: ${passed} passed, ${failed} failed`);
process.exit(failed > 0 ? 1 : 0);
