// Two players in one Anchor room must fight the same boss or mini-boss (the state-synced actors in
// soh/soh/Network/Anchor/ActorSync.cpp).
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/boss-sync-steps.mjs
//
// BOSSES=name,name picks some of the fights below (including ones marked `known`, which don't pass yet and are left out
// by default). For each, both players warp to its room and get past its intro;
// then the test checks that the fight looks the same on both, that a hit from B (who doesn't own the room) reaches A,
// and that a defeat on one starts the death on the other.
//
// Both players start a new file through a join link (slot 3), so no dungeon is cleared yet. Fights for adult Link
// switch both players to adult first (web_debug_set_age, like the Switch Age enhancement), with infinite health on.

import { startPlayers, synced, syncedHit, syncedPoke, syncedDefeat, syncedSetHealth, setCVar, dist, check } from './sync-test-lib.mjs';

const DMG_DEKU_NUT = 0x1;
const DMG_SLASH_KOKIRI = 0x100;
const DMG_SLASH_MASTER = 0x200;
const DMG_BIG_SWORD = 0x04000000;
const DMG_ARROW_NORMAL = 0x20;
const DMG_HAMMER_SWING = 0x40;

const LINK_AGE_ADULT = 0;
const LINK_AGE_CHILD = 1;

// A mini-boss B hurts with collider hits: the damage reaches A, and B's killing blow starts A's death too
const miniBoss = (name, entrance, age, room, pos, { dmg = DMG_SLASH_MASTER, part = 0, prepare, maxDistance, standOff = 0, defeat, known } = {}) => ({
    name,
    known,
    entrance,
    age,
    room,
    pos,
    maxDistance,
    standOff,
    findFloor: true,
    async hit(t) {
        if (prepare) await prepare(t);
        const before = await synced(t.A, name);
        const b = await t.until(t.B, (s) => s.health < before.health, `hurt the ${name} on B`, 20, () => syncedHit(t.B, name, 0, part, dmg));
        const a = await t.until(t.A, (s) => s.health <= b.health, `see B's hit on A (B ${b.health})`, 3);
        return { health: [before.health, b.health, a.health] };
    },
    async defeat(t) {
        if (defeat) return defeat(t);
        await syncedSetHealth(t.B, name, 0, 1);
        await t.until(t.B, (s) => s.defeated, `defeat the ${name} on B`, 20, () => syncedHit(t.B, name, 0, part, dmg));
    },
});

// Each fight: where it is, how to get past its intro, and its own hit and defeat checks. `hit` returns once B has
// hit it in a way that should show on A; `defeat` defeats it on B (or A) and returns who should see the death.
const FIGHTS = [
    {
        name: 'King Dodongo',
        entrance: 0x40b,
        age: LINK_AGE_CHILD,
        eventFlag: 0x71, // EVENTCHKINF_BEGAN_KING_DODONGO_BATTLE: the short intro
        pos: [-890, -1523, -3304], // dropping into the room starts the intro
        // B feeds him a bomb: he explodes and lies down on both
        async hit(t) {
            await t.until(t.B, (s) => s.action === 1 && t.poke(t.B), 'see King Dodongo breathe in on B', 60);
            await t.sleep(1500);
            const a = await synced(t.A, this.name);
            check([4, 5, 6].includes(a.action), `B's bomb didn't reach A (action ${a.action})`);
            return { actionOnA: a.action };
        },
        // A sword hit while he's down, on B
        async defeat(t) {
            await t.until(t.B, (s) => s.action === 5 || s.action === 6, 'see King Dodongo lie down on B', 60, async () => {
                const s = await synced(t.B, this.name);
                if (s.action === 1) await t.poke(t.B);
            });
            await syncedSetHealth(t.B, this.name, 0, 1);
            await t.until(t.B, (s) => s.action === -2, 'defeat King Dodongo on B', 20, () => syncedHit(t.B, this.name, 0, 0, DMG_SLASH_KOKIRI));
        },
    },
    {
        name: 'Phantom Ganon',
        known: "his def isn't registered: the horse ends up in another painting on B",
        entrance: 0x0c,
        age: LINK_AGE_ADULT,
        room: 1,
        eventFlag: 0x72, // EVENTCHKINF_BEGAN_PHANTOM_GANON_BATTLE: the short intro
        // The middle of the room, then in front of it, starts the intro
        pos: [14, -33, -3315],
        async enter(t) {
            await t.sleep(1000);
            for (const p of [t.A, t.B]) await p.evaluate(() => Module._web_debug_set_player_pos(14, -33, -3000));
        },
        // B's arrow hits him in a painting: he loses health and the horse turns back on A too
        async hit(t) {
            const before = await synced(t.A, this.name);
            const b = await t.until(t.B, (s) => s.health < before.health, 'hit Phantom Ganon on B', 40, () => syncedHit(t.B, this.name, 0, 0, DMG_ARROW_NORMAL));
            const a = await t.until(t.A, (s) => s.health <= b.health, `see B's hit on A (B ${b.health})`, 3);
            await t.until(t.A, (s) => s.members[0]?.action === 2, "see the horse turn back on A", 3);
            return { health: [before.health, b.health, a.health] };
        },
        // Out of the paintings after a few more arrows, then a sword
        async defeat(t) {
            await t.until(t.B, (s) => s.action > 0, 'get Phantom Ganon out of the paintings on B', 90, () => syncedHit(t.B, this.name, 0, 0, DMG_ARROW_NORMAL));
            await t.until(t.A, (s) => s.action > 0, 'see Phantom Ganon out of the paintings on A', 5);
            await syncedSetHealth(t.B, this.name, 0, 1);
            await t.until(t.B, (s) => s.defeated, 'defeat Phantom Ganon on B', 20, () => syncedHit(t.B, this.name, 0, 0, DMG_SLASH_MASTER));
        },
    },
    {
        name: 'Volvagia',
        entrance: 0x305,
        age: LINK_AGE_ADULT,
        eventFlag: 0x73, // EVENTCHKINF_BEGAN_VOLVAGIA_BATTLE
        pos: [340, 100, 0], // the intro starts here
        findFloor: true,
        // B hammers his head while it's out of a hole: it's knocked out on A too
        async hit(t) {
            const out = (s) => [2, 4, 5].includes(s.members[0]?.action); // Idle, breathing fire, swiping
            await t.until(t.B, out, "see Volvagia's head out on B", 90);
            const before = await synced(t.A, this.name);
            await t.until(t.B, (s) => s.members[0]?.action === 6, "knock Volvagia's head out on B", 10, () => syncedHit(t.B, this.name, 1, 0, DMG_HAMMER_SWING));
            const a = await t.until(t.A, (s) => s.members[0]?.action === 6 || s.members[0]?.action === 7, "see B's hammer on A", 3);
            return { health: [before.health, a.health] };
        },
        async defeat(t) {
            await syncedSetHealth(t.B, this.name, 0, 1);
            await t.until(t.B, (s) => s.defeated, 'defeat Volvagia on B', 60, async () => {
                const s = await synced(t.B, this.name);
                if (s?.members[0]?.action === 6) await syncedHit(t.B, this.name, 1, 0, DMG_SLASH_MASTER);
                else if ([2, 4, 5].includes(s?.members[0]?.action)) await syncedHit(t.B, this.name, 1, 0, DMG_HAMMER_SWING);
            });
        },
    },
    {
        name: 'Bongo Bongo',
        entrance: 0x413,
        age: LINK_AGE_ADULT,
        eventFlag: 0x77, // EVENTCHKINF_BEGAN_BONGO_BONGO_BATTLE: the short intro
        pos: [-50, 200, 200], // on the drum
        findFloor: true,
        // B hits his left hand: it reels on A too, and the head reacts
        async hit(t) {
            const REEL = 21, DAMAGED_HAND = 2;
            await t.until(t.B, (s) => s.members[0]?.action === REEL, "hit Bongo Bongo's hand on B", 40, () => syncedHit(t.B, this.name, 1, 0, DMG_SLASH_MASTER));
            const a = await t.until(t.A, (s) => s.members[0]?.action === REEL || s.action === DAMAGED_HAND, "see B's hit on A", 3);
            return { handOnA: a.members[0]?.action, headOnA: a.action };
        },
        // Defeated on B the way a defeat from someone else starts it
        async defeat(t) {
            await t.until(t.B, (s) => s.fighting, 'get Bongo Bongo fighting on B', 20);
            check(await syncedDefeat(t.B, this.name), "couldn't defeat Bongo Bongo on B");
        },
    },
    // Not right under it, where the camera can't see it hanging from the ceiling (so it never updates)
    miniBoss('Bari@2', 0x28, LINK_AGE_CHILD, 2, [3, 60, -3612], {
        known: "it stays at the ceiling on both games, so B's hits never land",
        standOff: 120,
        async prepare(t) {
            for (const p of [t.A, t.B]) await t.poke(p);
        },
    }),
    miniBoss('Lizalfos@1', 0x4, LINK_AGE_CHILD, 3, [3969, 100, -2161], {
        maxDistance: 100, // they hop fast
        // The pair take turns, and the one resting ignores hits: hit both, until one dies
        async defeat(t) {
            const pair = ['Lizalfos@1', 'Lizalfos@2'];
            for (const n of pair) await syncedSetHealth(t.B, n, 0, 1);
            for (let i = 0; i < 60 && !t.defeatedName; i++) {
                for (const n of pair) await syncedHit(t.B, n, 0, 0, DMG_SLASH_MASTER);
                await t.sleep(500);
                for (const n of pair) if ((await synced(t.B, n))?.defeated) t.defeatedName = n;
            }
            check(t.defeatedName, "couldn't defeat either Lizalfos on B");
        },
    }),
    miniBoss('Dead Hand', 0x98, LINK_AGE_CHILD, 4, [1281, -20, 211], {
        // One of its hands grabs B, which brings it up for both
        async prepare(t) {
            check(await t.poke(t.B), "Dead Hand wasn't waiting underground on B");
            await t.until(t.A, (s) => s.action === 1, 'see Dead Hand come up on A', 15);
        },
    }),
    miniBoss('Redead', 0x98, LINK_AGE_CHILD, 1, [565, -720, 251]),
    miniBoss('Iron Knuckle', 0x82, LINK_AGE_ADULT, 10, [-1250, 1003, 1476], { dmg: DMG_BIG_SWORD }),
    miniBoss('Stalfos', 0x169, LINK_AGE_ADULT, 6, [-38, 467, -3382], { maxDistance: 100 }), // jump slashes are fast
    miniBoss('Wolfos@1', 0x169, LINK_AGE_ADULT, 0, [161, 309, 393], {
        known: 'follows exactly when run first, but drifts after another fight in the same session',
    }),
    miniBoss('Freezard@11', 0x88, LINK_AGE_ADULT, 1, [0, -10, 1078], { dmg: DMG_BIG_SWORD }),
];

export default async ({ page, sleep, log }) => {
    const only = (process.env.BOSSES || '').split(',').filter(Boolean);
    // Fights with a known problem only run when asked for by name
    const fights = FIGHTS.filter((f) => (only.length ? only.includes(f.name) : !f.known));
    check(fights.length, `no fights match BOSSES=${process.env.BOSSES}`);
    const { A, B, close } = await startPlayers({ page, sleep }, 'soh-boss-test-');
    const results = {};
    try {
        for (const fight of fights) {
            const t = {
                A,
                B,
                sleep,
                log,
                poke: (p, member = 0) => syncedPoke(p, fight.name, member),
                // Polls one player's view of the fight until done(state), doing `each` between polls
                async until(p, done, what, seconds, each) {
                    let s;
                    for (let i = 0; i < seconds * 2; i++) {
                        s = await synced(p, fight.name);
                        if (s && (await done(s))) return s;
                        if (each) await each();
                        await sleep(500);
                    }
                    const other = await synced(p === A ? B : A, fight.name);
                    throw new Error(`couldn't ${what} (${p === A ? 'A' : 'B'} ${JSON.stringify(s)}, other ${JSON.stringify(other)})`);
                },
            };
            const result = {};
            try {
                // Getting there
                for (const p of [A, B]) {
                    await p.evaluate((age) => Module._web_debug_set_age(age), fight.age);
                    await p.evaluate((e) => Module._web_debug_warp(e), fight.entrance);
                }
                await sleep(9000);
                if (fight.room !== undefined) {
                    for (const p of [A, B]) check(await p.evaluate((r) => Module._web_debug_load_room(r), fight.room), 'room did not load');
                    await sleep(3000);
                }
                // Mini-bosses: somewhere to stand near the spot given (not in a pit), so that Link doesn't void out
                let pos = fight.pos;
                if (fight.findFloor) {
                    const floor = (x, y, z) => A.evaluate(([x, y, z]) => JSON.parse(UTF8ToString(Module._web_debug_floor(x, y, z))), [x, y, z]);
                    pos = null;
                    for (const r of [0, 80, 160, 240, 320].map((r) => r + (fight.standOff ?? 0))) {
                        for (let i = 0; i < (r ? 8 : 1) && !pos; i++) {
                            const [x, z] = [fight.pos[0] + r * Math.cos(i * Math.PI / 4), fight.pos[2] + r * Math.sin(i * Math.PI / 4)];
                            const f = await floor(x, fight.pos[1] + 100, z);
                            if (f.y > fight.pos[1] - 1200 && !f.pit) pos = [x, f.y + 5, z];
                        }
                    }
                    check(pos, `no floor near ${fight.pos}`);
                }
                for (const p of [A, B]) {
                    if (fight.eventFlag !== undefined) await p.evaluate((f) => Module._web_debug_set_event_flag(f), fight.eventFlag);
                    await setCVar(p, 'gCheats.InfiniteHealth', 1);
                    await p.evaluate(([x, y, z]) => Module._web_debug_set_player_pos(x, y, z), pos);
                }
                if (fight.enter) await fight.enter(t);

                // Both fighting the same fight
                let samples = 0, worst = 0, last;
                for (let i = 0; i < 90 && samples < 6; i++) {
                    await sleep(1000);
                    last = [await synced(A, fight.name), await synced(B, fight.name)];
                    const [sa, sb] = last;
                    if (sa?.fighting && sb?.fighting) {
                        samples++;
                        worst = Math.max(worst, dist(sa, sb));
                    }
                }
                check(samples >= 6, `${fight.name} never got past the intro on both (${JSON.stringify(last)})`);
                const [sa, sb] = [await synced(A, fight.name), await synced(B, fight.name)];
                check(sa.owner && !sb.owner, `expected A to own the room (A ${sa.owner}, B ${sb.owner})`);
                check(sa.room === sb.room && sa.index === sb.index, `A and B look at different actors (${sa.index}, ${sb.index})`);
                check(worst < (fight.maxDistance ?? 60), `positions differ by up to ${worst.toFixed(1)} (A ${JSON.stringify(sa)}, B ${JSON.stringify(sb)})`);
                result.worstDistance = +worst.toFixed(1);

                result.hit = (await fight.hit(t)) ?? true;
                await fight.defeat(t);
                const other = fight.defeatedOn === 'A' ? B : A;
                // A death that's over is gone from the list
                let seen = false;
                for (let i = 0; i < 20 && !seen; i++) {
                    const s = await synced(other, t.defeatedName ?? `${fight.name.split('@')[0]}@${sa.index}`);
                    seen = !s || s.defeated;
                    if (!seen) await sleep(250);
                }
                check(seen, `the defeat didn't reach ${fight.defeatedOn === 'A' ? 'B' : 'A'}`);
                result.defeat = true;
                log(fight.name, JSON.stringify(result));
            } catch (e) {
                result.error = String(e.message ?? e);
                log(fight.name, 'FAILED:', result.error);
            }
            results[fight.name] = result;
        }
    } finally {
        await close();
    }
    const failed = Object.entries(results).filter(([, r]) => r.error);
    check(!failed.length, `failed: ${failed.map(([n, r]) => `${n} (${r.error})`).join('; ')}`);
    return results;
};
