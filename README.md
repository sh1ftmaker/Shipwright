![Ship of Harkinian](docs/shiptitle.darkmode.png#gh-dark-mode-only)
![Ship of Harkinian](docs/shiptitle.lightmode.png#gh-light-mode-only)

# Ship of Harkinian — Web Edition

Play Ocarina of Time in your browser. This is an Emscripten/WebAssembly fork of [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) that runs entirely client-side.

**Play now: [sh1ftmaker.github.io/Shipwright/](https://sh1ftmaker.github.io/Shipwright/)**

> This is an independent fork. The upstream project has no plans to merge web support.

## What's Different

This fork adds:

- **Emscripten/WASM build** — the full game compiled to WebAssembly with WebGL rendering
- **Browser ROM loading** — upload `.o2r` files or load them from URLs via hash parameters
- **URL-based configuration** — pass game archives, multiplayer room, player name, and color all via URL
- **Anchor multiplayer** — the web build joins the same Anchor rooms as desktop SoH, through a small WebSocket bridge
- **Mobile support** — touch gamepad with analog stick and all N64 buttons, scaled UI for small screens, iOS audio compatibility, and persistent save files via IndexedDB
- **IndexedDB persistence** — game archives cached on first load, save files persisted automatically
- **Cutscene skipping** — all TimeSaver skip enhancements enabled by default

## Quick Start

You need two archive files: `soh.o2r` (game engine assets) and `oot.o2r` (extracted ROM data). These are not included — you must provide a legally acquired ROM.

### Option 1: Upload files manually
Visit [sh1ftmaker.github.io/Shipwright/](https://sh1ftmaker.github.io/Shipwright/) and drag/drop your `soh.o2r` and `oot.o2r` files.

### Option 2: Load via URL parameters
```
https://sh1ftmaker.github.io/Shipwright/#soh=<SOH_URL>&oot=<OOT_URL>
```

### Option 3: Local file server
```bash
./serve_o2r.sh PlayerName
```
This starts a CORS-enabled file server with a Cloudflare tunnel and prints a ready-to-use URL.

## URL Parameters

All parameters are passed via the URL hash (`#key=value&key2=value2`):

| Parameter | Description | Default |
|-----------|-------------|---------|
| `rom` | URL to a ROM (`.z64`/`.n64`/`.v64`), converted in the browser | — |
| `soh` | URL to `soh.o2r` archive | — |
| `oot` | URL to `oot.o2r` archive | — |
| `room` | Multiplayer room name | the last one used (the public room at first) |
| `name` | Player display name | — |
| `color` | Player color (hex RGB) | `64FF64` |
| `team` | Team ID for item/flag sync | `default` |
| `anchor` | WebSocket URL of a different Anchor bridge | built-in bridge |
| `join` | `1`: connect, skip the menus, load a save and go to the room owner | — |
| `slot` | Save slot (1-3) a join link loads, created if empty | `3` |

`room`, `name`, `color` and `team` fill in Network > Anchor, and a link with a `room` connects straight away
(Anchor is on by default; without a link the game joins the public room). A join link such as `#room=myroom&name=Navi&join=1` skips the title and file select, loads
(or creates) the save in `slot`, syncs the team's progress and teleports to the room owner. It also turns on
Skip Intro, so a new file doesn't start with the opening cutscene. URLs in these
parameters must be URL-encoded if they contain `&` or `#`.

A link without a `name` or `room`, such as the quick invite `#oot=<url>&room=myroom&join=1`, opens a profile
dialog before the game starts: name, Link's color (unless the link sets `color`), room and save file, filled in with
what that browser used last. It also shows the invite link for the room, which leaves the name out so each friend
picks their own. A reload of the same link, or a plain visit by someone who has picked a profile before, skips it.

## Multiplayer

Anchor (Network > Anchor) works as on desktop: players in the same room see each other, and players on the same
team share items and flags. Desktop builds connect to the Anchor server (`anchor.hm64.org:43383`) over TCP, which
browsers can't do, so the web build connects to a Cloudflare Worker that relays its WebSocket to that server
(`soh/soh/web/anchor-bridge/`; each connection runs in its own Durable Object, which the free plan allows). Web and desktop players therefore share rooms, though items and flags only sync
between identical builds (Anchor compares git commit hashes).

Players on the same web build also see the same enemies and push blocks (Room Settings > Sync Enemies & Push
Blocks, on by default). In each map room, the player who has been there longest runs the enemies, everyone else
follows their positions, and damage, kills and block pushes from anyone reach everyone
(`soh/soh/Network/Anchor/ActorSync.cpp`). Someone who walks in, or comes back after leaving or reloading, follows the
fight already going on; when the player running it leaves, whoever has been there longest after them takes over.
The Anchor room itself works the same way: when its owner has been gone for 5 seconds a remaining player takes it
over, and the old owner comes back as a regular player.
Bosses and mini-bosses are synced as state machines: the room owner sends the whole fight state, so the boss does the
same things at the same moments for everyone, and anyone's hits count; intros and the death cutscene play for each
player. Only actors from a room's spawn list are synced (plus a few that a spawner makes), not ones spawned during
play.

Builds use `wss://soh-anchor-bridge.sh1ftmaker.workers.dev` by default. To run your own, deploy the bridge to your
Cloudflare account (free plan is fine) and build with its URL:
```bash
cd soh/soh/web/anchor-bridge && npm install && npx wrangler deploy
cmake -B build-web ... -DSOH_WEB_ANCHOR_URL=wss://soh-anchor-bridge.<your-subdomain>.workers.dev
```
`npx wrangler dev` runs it locally (`#anchor=ws://127.0.0.1:8787` points the game at it).

The earlier PartyKit relay was retired along with PartyKit's hosted platform (October 2026).

## Building

### Prerequisites
- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (3.1.64+)
- CMake 3.20+
- Ninja

### Build
```bash
emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
emmake cmake --build build-web --parallel $(nproc)
```

### Serve locally
```bash
python3 serve_web.py
# Open http://localhost:8080/soh.html
```

## Keyboard Controls

| N64 | A | B | Z | Start | Analog stick | C buttons | D-Pad |
| - | - | - | - | - | - | - | - |
| Keyboard | X | C | Z | Space | WASD | Arrow keys | TFGH |

| Keys | Action |
| - | - |
| ESC | Toggle menu |
| F11 | Fullscreen |
| Ctrl+R | Reset |

## Credits

Based on [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) by HarbourMasters.

- [Official SoH Website](https://www.shipofharkinian.com/)
- [Official SoH Discord](https://discord.com/invite/shipofharkinian)
- [Credits](docs/CREDITS.md)

<a href="https://github.com/Kenix3/libultraship/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/poweredbylus.darkmode.png">
    <img alt="Powered by libultraship" src="./docs/poweredbylus.lightmode.png">
  </picture>
</a>
