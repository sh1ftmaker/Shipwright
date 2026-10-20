// Two players in one Anchor room must fight the same boss or mini-boss (the state-synced actors in
// soh/soh/Network/Anchor/ActorSync.cpp).
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/boss-sync-steps.mjs
//
// BOSSES=name,name picks some of the fights below. For each, both players warp to its room and get past its intro;
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

const LINK_AGE_ADULT = 0;
const LINK_AGE_CHILD = 1;

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
];

export default async ({ page, sleep, log }) => {
    const only = (process.env.BOSSES || '').split(',').filter(Boolean);
    const fights = FIGHTS.filter((f) => !only.length || only.includes(f.name));
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
                    for (let i = 0; i < seconds * 2; i++) {
                        const s = await synced(p, fight.name);
                        if (s && (await done(s))) return s;
                        if (each) await each();
                        await sleep(500);
                    }
                    throw new Error(`couldn't ${what}`);
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
                for (const p of [A, B]) {
                    if (fight.eventFlag !== undefined) await p.evaluate((f) => Module._web_debug_set_event_flag(f), fight.eventFlag);
                    await setCVar(p, 'gCheats.InfiniteHealth', 1);
                    await p.evaluate(([x, y, z]) => Module._web_debug_set_player_pos(x, y, z), fight.pos);
                }

                // Both fighting the same fight
                let samples = 0, worst = 0;
                for (let i = 0; i < 90 && samples < 6; i++) {
                    await sleep(1000);
                    const [sa, sb] = [await synced(A, fight.name), await synced(B, fight.name)];
                    if (sa?.fighting && sb?.fighting) {
                        samples++;
                        worst = Math.max(worst, dist(sa, sb));
                    }
                }
                check(samples >= 6, `${fight.name} never got past the intro on both`);
                const [sa, sb] = [await synced(A, fight.name), await synced(B, fight.name)];
                check(sa.owner && !sb.owner, `expected A to own the room (A ${sa.owner}, B ${sb.owner})`);
                check(worst < (fight.maxDistance ?? 60), `positions differ by up to ${worst.toFixed(1)}`);
                result.worstDistance = +worst.toFixed(1);

                result.hit = (await fight.hit(t)) ?? true;
                await fight.defeat(t);
                const other = fight.defeatedOn === 'A' ? B : A;
                await t.until(other, (s) => s.action === -2 || s.defeated, `see the defeat on ${fight.defeatedOn === 'A' ? 'B' : 'A'}`, 5);
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
