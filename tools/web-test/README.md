# Web build gameplay test

Drives the Emscripten build in Chromium: extracts a ROM in-browser with Torch,
boots to the title screen, creates and starts a save file, walks Link around,
and measures frame pacing.

```bash
cd tools/web-test && npm install && cd ../..
./build_web.sh                      # or an existing build-web/soh + build-tools/soh/soh.o2r
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --fps 20,30,60   # check the FPS setting
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --url https://sh1ftmaker.github.io/Shipwright/
# two players in one Anchor room: enemies, damage, kills and push blocks must match
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --included-save --steps tools/web-test/actor-sync-steps.mjs
# Anchor connection drops, offline save changes, room ownership, reloads and hidden tabs
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --included-save --steps tools/web-test/reconnect-steps.mjs
# two players: switches, temp flags, keys, chest races and the team state merge
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --included-save --steps tools/web-test/world-sync-steps.mjs
# players leaving and coming back: room and enemy ownership moves to whoever stays
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --included-save --steps tools/web-test/handover-steps.mjs
# the profile dialog a quick invite link opens
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --included-save --steps tools/web-test/profile-steps.mjs
```

Screenshots and `report.json` (step results, FPS samples, console log) go to
`web-test-results/`. Exit code is 0 only if every step passed.

## How it works

- **Display**: headless Chromium usually has no WebGL2, so by default the script
  starts an offscreen `weston --backend=headless --renderer=gl` compositor and runs
  Chromium inside it (GPU-backed, no visible window). `--display headless` skips that.
- **Input**: sets `TouchGamepad` state in the page, which `web_main.cpp` merges into
  the N64 controller pad every frame — the same path real touch input takes.
- **State**: reads `web_debug_get(field)` from `soh/src/code/graph.c` (game state,
  scene, Link's position, tick/render/rAF counters, measured FPS, R_UPDATE_RATE).
  Keep the field table at the top of the script in sync with it.
- **Warps**: `web_debug_warp(entrance)` in `graph.c` moves Link to another entrance, like the
  `warp` console command (the test uses it to get from Link's house to Kokiri Forest, where the
  camera follows Link).
- **Devices**: `--device phone-portrait` / `phone-landscape` emulate a touch phone and press the
  on-screen gamepad with real touch events.
- **Saves**: the page installs the save it ships with (`soh/soh/web/soh-save-*.zip`) on a browser's first
  visit. The test opens the page with `?no-included-save` so it starts with empty slots and creates file 1
  itself; `--included-save` turns that off.
- **Settings**: `web_cvar_set_int(name, value)` in `web_main.cpp` sets a CVar and
  re-runs its ShipInit hooks (the test uses it to skip the intro cutscene).

## FPS numbers

Each sample reports browser frames (`rafPerSec`), game logic ticks
(`ticksPerSec`, 20 in gameplay), interpolated renders (`rendersPerSec`), frames
that drew anything (`presentsPerSec`), and page-side frame-time percentiles.
The weston display runs at 60 Hz.

`--fps` turns off Match Refresh Rate and checks that each Settings > Graphics
FPS value gives that many frames per second (capped at the display rate) while
the game still ticks at 20 Hz. 20 is the original rate: one frame per tick, no
interpolation.
