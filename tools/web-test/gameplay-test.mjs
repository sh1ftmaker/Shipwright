#!/usr/bin/env node
// Automated gameplay test for the web (Emscripten) build.
//
// Boots the build in Chromium, extracts a ROM in-browser with Torch, starts a new
// save file, walks Link around, and measures frame pacing. Input goes through the
// touch-gamepad bridge (shell.html TouchGamepad -> web_main.cpp -> padmgr.c), and
// assertions read game state through web_debug_get() (graph.c).
//
// Usage:
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> [options]
//
// Options:
//   --rom <path>        ROM to extract (required)
//   --build <dir>       Web build output to serve (default: build-web/soh)
//   --soh-o2r <path>    soh.o2r to serve if the build dir lacks one (default: build-tools/soh/soh.o2r)
//   --url <url>         Test an already-hosted page instead of serving --build
//   --out <dir>         Screenshots + report.json (default: web-test-results)
//   --chromium <path>   Chromium binary (default: $CHROMIUM or /usr/bin/chromium)
//   --display <mode>    auto | weston | headless (default: auto). Headless Chromium has no
//                       WebGL2 on many machines; "weston" runs a GPU-backed offscreen compositor.
//   --fps-seconds <n>   Length of each frame-pacing sample (default: 10)
//   --fps <list>        InterpolationFPS values to check with Match Refresh Rate off, e.g. "20,30,60"
//                       (20 = original: one frame per game tick, no interpolation)
//
// Exit code is 0 only if every step passed.

import { chromium } from 'playwright-core';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { setTimeout as sleep } from 'node:timers/promises';

// ---- args ----
const args = parseArgs(process.argv.slice(2));
if (!args.rom) {
    console.error('usage: gameplay-test.mjs --rom <rom.z64> [--build dir] [--url url] [--out dir]');
    process.exit(2);
}
const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), '../..');
const BUILD_DIR = path.resolve(args.build ?? path.join(ROOT, 'build-web/soh'));
const SOH_O2R = path.resolve(args['soh-o2r'] ?? path.join(ROOT, 'build-tools/soh/soh.o2r'));
const OUT = path.resolve(args.out ?? 'web-test-results');
const CHROMIUM = args.chromium ?? process.env.CHROMIUM ?? '/usr/bin/chromium';
const FPS_SECONDS = Number(args['fps-seconds'] ?? 10);
const EXTRA_FPS = (args.fps ?? '').split(',').filter(Boolean).map(Number);

fs.mkdirSync(OUT, { recursive: true });

// ---- debug fields (keep in sync with web_debug_get in soh/src/code/graph.c) ----
const F = {
    scene: 0, posX: 1, posY: 2, posZ: 3, ticks: 4, renders: 5, rafs: 6, measuredFps: 7,
    updateRate: 8, entrance: 9, fileNum: 10, room: 11, health: 12, gameState: 13, presents: 14, targetFps: 15,
};
const GAME_STATE = { TitleSetup: 0, Select: 1, Title: 2, Play: 3, Opening: 4, FileChoose: 5 };

// ---- reporting ----
const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(1).padStart(6) + 's';
const log = (...a) => console.log(elapsed(), ...a);
const report = { rom: path.basename(args.rom), started: new Date().toISOString(), steps: [], fps: [], console: [] };

let page;
let failed = false;

async function step(name, fn) {
    const start = Date.now();
    log(`▶ ${name}`);
    try {
        const detail = await fn();
        const secs = (Date.now() - start) / 1000;
        report.steps.push({ name, ok: true, secs, detail });
        log(`✔ ${name} (${secs.toFixed(1)}s)${detail ? ' ' + JSON.stringify(detail) : ''}`);
        return detail;
    } catch (e) {
        const secs = (Date.now() - start) / 1000;
        report.steps.push({ name, ok: false, secs, error: String(e.message ?? e) });
        log(`✘ ${name}: ${e.message ?? e}`);
        failed = true;
        await shot(`FAIL-${slug(name)}`);
        throw e;
    }
}

// ---- game helpers ----
const dbg = (field) => page.evaluate((f) => Module._web_debug_get(f), field);
async function state() {
    const s = await page.evaluate((F) => {
        const o = {};
        for (const [k, v] of Object.entries(F)) o[k] = Module._web_debug_get(v);
        return o;
    }, F);
    s.pos = [s.posX, s.posY, s.posZ].map((v) => Math.round(v));
    return s;
}

async function waitFor(desc, pred, timeoutMs = 30000, pollMs = 250) {
    const end = Date.now() + timeoutMs;
    let s;
    while (Date.now() < end) {
        s = await state();
        if (pred(s)) return s;
        await sleep(pollMs);
    }
    throw new Error(`timed out waiting for ${desc}; last state ${JSON.stringify(s)}`);
}

// Game ticks rather than wall time, so slow machines still give the game enough frames.
async function waitTicks(n) {
    const start = await dbg(F.ticks);
    const end = Date.now() + Math.max(10000, n * 200);
    while ((await dbg(F.ticks)) - start < n) {
        if (Date.now() > end) throw new Error(`game stalled: fewer than ${n} ticks`);
        await sleep(50);
    }
}

const BUTTONS = { A: 'tb-a', B: 'tb-b', Z: 'tb-z', START: 'tb-start', L: 'tb-l', R: 'tb-r', CU: 'tb-cu', CD: 'tb-cd', CL: 'tb-cl', CR: 'tb-cr' };
const pad = {
    async set(buttons = [], stickX = 0, stickY = 0) {
        await page.evaluate(([ids, x, y]) => {
            TouchGamepad._active = true;
            TouchGamepad._buttons = {};
            for (const id of ids) TouchGamepad._buttons[id] = true;
            TouchGamepad._stickX = x;
            TouchGamepad._stickY = y;
        }, [buttons.map((b) => BUTTONS[b]), stickX, stickY]);
    },
    release() {
        return this.set();
    },
    // Hold for a few game ticks so the press is seen regardless of tick rate.
    async press(button, holdTicks = 3, gapTicks = 3) {
        await this.set([button]);
        await waitTicks(holdTicks);
        await this.release();
        await waitTicks(gapTicks);
    },
    async stick(x, y, ticks) {
        await this.set([], x, y);
        await waitTicks(ticks);
        await this.release();
    },
};

async function setCVar(name, value) {
    await page.evaluate(([n, v]) => {
        const p = Module.allocateUTF8(n);
        Module._web_cvar_set_int(p, v);
        Module._free(p);
    }, [name, value]);
}

async function shot(name) {
    const file = path.join(OUT, `${String(report.steps.length).padStart(2, '0')}-${slug(name)}.png`);
    await page.screenshot({ path: file, timeout: 120000 }).catch((e) => log('screenshot failed:', e.message));
    return path.basename(file);
}

// Frame pacing over a wall-clock window: browser frames (rAF), game ticks, interpolated renders and
// frames that drew anything (presents) per second, plus rAF frame-time percentiles on the page side.
async function measureFps(label) {
    await page.evaluate(() => {
        window.__frameTimes = [];
        let last = performance.now();
        const loop = (t) => {
            window.__frameTimes.push(t - last);
            last = t;
            if (window.__frameTimes.length < 100000) window.__frameRaf = requestAnimationFrame(loop);
        };
        window.__frameRaf = requestAnimationFrame(loop);
    });
    const a = await state();
    const wallStart = Date.now();
    await sleep(FPS_SECONDS * 1000);
    const b = await state();
    const secs = (Date.now() - wallStart) / 1000;
    const times = await page.evaluate(() => {
        cancelAnimationFrame(window.__frameRaf);
        return window.__frameTimes.slice(1);
    });
    const sorted = [...times].sort((x, y) => x - y);
    const pct = (p) => +(sorted[Math.min(sorted.length - 1, Math.floor(p * sorted.length))] ?? 0).toFixed(1);
    const r = {
        label,
        secs: +secs.toFixed(1),
        rafPerSec: +((b.rafs - a.rafs) / secs).toFixed(1),
        ticksPerSec: +((b.ticks - a.ticks) / secs).toFixed(1),
        rendersPerSec: +((b.renders - a.renders) / secs).toFixed(1),
        presentsPerSec: +((b.presents - a.presents) / secs).toFixed(1),
        targetFps: b.targetFps,
        measuredFps: b.measuredFps,
        updateRate: b.updateRate,
        frameMs: { p50: pct(0.5), p95: pct(0.95), p99: pct(0.99), max: pct(1) },
        longFrames: times.filter((t) => t > 50).length,
    };
    report.fps.push(r);
    log('fps', JSON.stringify(r));
    return r;
}

// ---- infrastructure ----
function serve(dir) {
    const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream', '.o2r': 'application/octet-stream' };
    const server = http.createServer((req, res) => {
        let rel = decodeURIComponent(new URL(req.url, 'http://x').pathname);
        if (rel === '/') rel = '/soh.html';
        let file = path.join(dir, path.normalize(rel));
        if (!file.startsWith(dir)) return res.writeHead(403).end();
        if (rel === '/soh.o2r' && !fs.existsSync(file) && fs.existsSync(SOH_O2R)) file = SOH_O2R;
        fs.stat(file, (err, st) => {
            if (err || !st.isFile()) return res.writeHead(404).end();
            res.writeHead(200, { 'Content-Type': types[path.extname(file)] ?? 'application/octet-stream', 'Content-Length': st.size });
            fs.createReadStream(file).pipe(res);
        });
    });
    return new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(server)));
}

async function startWeston() {
    const runtime = process.env.XDG_RUNTIME_DIR ?? `/run/user/${process.getuid()}`;
    const socket = `soh-web-test-${process.pid}`;
    const proc = spawn('weston', ['--backend=headless', '--renderer=gl', `--socket=${socket}`, '--width=1280', '--height=720'], {
        env: { ...process.env, XDG_RUNTIME_DIR: runtime },
        stdio: 'ignore',
    });
    proc.on('error', () => {});
    for (let i = 0; i < 40; i++) {
        if (fs.existsSync(path.join(runtime, socket))) return { proc, socket, runtime };
        if (proc.exitCode !== null) break;
        await sleep(250);
    }
    proc.kill();
    return null;
}

async function launchBrowser(mode) {
    const base = ['--autoplay-policy=no-user-gesture-required', '--ignore-gpu-blocklist', '--window-size=1280,720'];
    if (mode !== 'headless') {
        const weston = await startWeston();
        if (weston) {
            const browser = await chromium.launch({
                executablePath: CHROMIUM,
                headless: false,
                env: { ...process.env, XDG_RUNTIME_DIR: weston.runtime, WAYLAND_DISPLAY: weston.socket, DISPLAY: '' },
                args: [...base, '--ozone-platform=wayland'],
            });
            return { browser, cleanup: () => weston.proc.kill(), display: 'weston' };
        }
        if (mode === 'weston') throw new Error('could not start weston (is it installed?)');
    }
    const browser = await chromium.launch({
        executablePath: CHROMIUM,
        args: [...base, '--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
    });
    return { browser, cleanup: () => {}, display: 'headless' };
}

function parseArgs(argv) {
    const o = {};
    for (let i = 0; i < argv.length; i++) {
        if (argv[i].startsWith('--')) o[argv[i].slice(2)] = argv[i + 1]?.startsWith('--') || argv[i + 1] === undefined ? true : argv[++i];
    }
    return o;
}

function slug(s) {
    return s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
}

// ---- the test ----
let server;
let browser;
let cleanup = () => {};
try {
    let url = args.url;
    if (!url) {
        if (!fs.existsSync(path.join(BUILD_DIR, 'soh.html'))) throw new Error(`no web build at ${BUILD_DIR}`);
        server = await serve(BUILD_DIR);
        url = `http://127.0.0.1:${server.address().port}/soh.html`;
    }
    ({ browser, cleanup, display: report.display } = await launchBrowser(args.display ?? 'auto'));
    log(`page ${url} (display: ${report.display})`);

    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    page = await ctx.newPage();
    page.on('console', (m) => {
        const text = m.text();
        if (/OTR written|Registered factory|RandomizerInf Flag Set/.test(text)) return;
        report.console.push(`${elapsed()} [${m.type()}] ${text}`);
        if (m.type() === 'error' || /\[(error|critical)\]/.test(text)) log('console:', text.slice(0, 200));
    });
    page.on('pageerror', (e) => {
        report.console.push(`${elapsed()} [pageerror] ${e.stack ?? e.message}`);
        log('pageerror:', e.message);
        failed = true;
    });
    page.on('crash', () => {
        log('page crashed');
        failed = true;
    });
    await page.route('**/__web_test_rom.z64', (r) => r.fulfill({ body: fs.readFileSync(args.rom) }));

    await step('page loads', async () => {
        await page.goto(url);
        await page.waitForFunction(() => typeof Module !== 'undefined' && Module.calledRun && typeof Module._web_debug_get === 'function', null, { timeout: 120000 });
        const webgl = await page.evaluate(() => !!document.createElement('canvas').getContext('webgl2'));
        if (!webgl) throw new Error('no WebGL2 in this browser (try --display weston)');
    });

    await step('extract ROM with Torch', async () => {
        const start = Date.now();
        await page.evaluate(async () => {
            const buf = await (await fetch('/__web_test_rom.z64')).arrayBuffer();
            extractRomInBrowser(buf, 'rom.z64');
        });
        await page.waitForFunction(() => {
            try {
                const f = FS.readdir('/');
                return f.includes('oot.o2r') || f.includes('oot-mq.o2r');
            } catch (e) {
                return false;
            }
        }, null, { timeout: 300000, polling: 500 });
        const files = await page.evaluate(() => FS.readdir('/').filter((f) => f.endsWith('.o2r')).map((f) => `${f} ${FS.stat('/' + f).size}`));
        return { secs: +((Date.now() - start) / 1000).toFixed(1), files };
    });

    await step('boot to title screen', async () => {
        const s = await waitFor('title demo (Play state)', (s) => s.gameState === GAME_STATE.Play && s.ticks > 60, 120000);
        await shot('title');
        return { scene: s.scene, updateRate: s.updateRate };
    });

    await measureFps('title screen');

    await step('press Start -> file select', async () => {
        // Skip the opening cutscene on the new file so we land in Link's house in control.
        await setCVar('gEnhancements.TimeSavers.SkipCutscene.Intro', 1);
        for (let i = 0; i < 6; i++) {
            await pad.press('START');
            const s = await state();
            if (s.gameState === GAME_STATE.FileChoose) break;
        }
        await waitFor('FileChoose state', (s) => s.gameState === GAME_STATE.FileChoose, 20000);
        await waitTicks(40);
        await shot('file-select');
    });

    // File select runs at 60 ticks/s and ignores input during its fades, so leave long gaps.
    await step('create file 1', async () => {
        await pad.press('A', 4, 60); // empty slot -> quest select (SoH, when MQ is available) or name entry
        await shot('quest-or-name');
        await pad.press('A', 4, 60); // pick the quest; on a vanilla-only setup this just types a letter
        await pad.press('A', 4, 20); // type the letter under the cursor
        await shot('name-entry');
        await pad.press('START', 4, 30); // jump to END
        await pad.press('A', 4, 90); // confirm name, back to file select
        await shot('file-created');
    });

    await step('start file 1', async () => {
        await pad.press('A', 4, 60); // select file 1
        await shot('file-confirm');
        await pad.press('A', 4, 10); // "Yes"
        const s = await waitFor('gameplay (Play state, in control)', (s) => s.gameState === GAME_STATE.Play && s.fileNum === 0 && s.health > 0, 60000, 500);
        await waitTicks(60);
        await shot('in-game');
        return { scene: s.scene, entrance: s.entrance, pos: s.pos };
    });

    await step('walk with the analog stick', async () => {
        // Dismiss any leftover text boxes first.
        for (let i = 0; i < 4; i++) await pad.press('B', 2, 4);
        const before = await state();
        await pad.stick(0, 80, 40); // forward
        const mid = await state();
        await pad.stick(70, 0, 30); // right
        const after = await state();
        await shot('after-walking');
        const dist = (a, b) => Math.hypot(a.posX - b.posX, a.posZ - b.posZ);
        const moved = dist(before, mid) + dist(mid, after);
        if (moved < 50) throw new Error(`Link barely moved (${moved.toFixed(1)} units): ${JSON.stringify([before.pos, mid.pos, after.pos])}`);
        return { moved: Math.round(moved), path: [before.pos, mid.pos, after.pos] };
    });

    await measureFps('gameplay (default settings)');
    if (EXTRA_FPS.length) {
        await step('FPS setting changes frame rate', async () => {
            const results = {};
            const errors = [];
            await setCVar('gSettings.MatchRefreshRate', 0);
            for (const fps of EXTRA_FPS) {
                await setCVar('gSettings.InterpolationFPS', fps);
                await waitTicks(20);
                await pad.set([], 0, 70); // keep Link moving so interpolation has work to do
                const r = await measureFps(`gameplay InterpolationFPS=${fps}`);
                await shot(`fps-${fps}`);
                await pad.release();
                // 20 = original: one frame per 20 Hz tick. Otherwise the setting, capped at the display rate.
                const expected = fps === 20 ? r.ticksPerSec : Math.min(fps, r.rafPerSec);
                results[fps] = { presentsPerSec: r.presentsPerSec, expected, ticksPerSec: r.ticksPerSec };
                if (Math.abs(r.presentsPerSec - expected) > expected * 0.1 + 1) {
                    errors.push(`InterpolationFPS=${fps}: ${r.presentsPerSec} frames/s, expected ~${expected}`);
                }
                if (Math.abs(r.ticksPerSec - 20) > 2) errors.push(`InterpolationFPS=${fps}: game ran at ${r.ticksPerSec} ticks/s`);
            }
            await setCVar('gSettings.MatchRefreshRate', 1);
            if (errors.length) throw new Error(errors.join('; '));
            return results;
        });
    }
} catch (e) {
    if (!report.steps.some((s) => !s.ok)) {
        report.steps.push({ name: 'harness', ok: false, error: String(e.stack ?? e) });
        log('harness error:', e.stack ?? e);
    }
    failed = true;
} finally {
    report.passed = !failed;
    fs.writeFileSync(path.join(OUT, 'report.json'), JSON.stringify(report, null, 2));
    await browser?.close().catch(() => {});
    cleanup();
    server?.close();
    log(failed ? 'FAILED' : 'PASSED', `- report: ${path.join(OUT, 'report.json')}`);
    process.exit(failed ? 1 : 0);
}
