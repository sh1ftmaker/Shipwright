# Web build gameplay test

Drives the Emscripten build in Chromium: extracts a ROM in-browser with Torch,
boots to the title screen, creates and starts a save file, walks Link around,
and measures frame pacing.

```bash
cd tools/web-test && npm install && cd ../..
./build_web.sh                      # or an existing build-web/soh + build-tools/soh/soh.o2r
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --fps 20,30,60   # sample InterpolationFPS values
node tools/web-test/gameplay-test.mjs --rom path/to/oot.z64 --url https://sh1ftmaker.github.io/Shipwright/
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
- **Settings**: `web_cvar_set_int(name, value)` in `web_main.cpp` sets a CVar and
  re-runs its ShipInit hooks (the test uses it to skip the intro cutscene).

## FPS numbers

Each sample reports browser frames (`rafPerSec`), game logic ticks
(`ticksPerSec`, 20 in gameplay), interpolated renders (`rendersPerSec`), and
page-side frame-time percentiles. The weston display runs at 60 Hz.
