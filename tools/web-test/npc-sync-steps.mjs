// Two players in one Anchor room must see the same townspeople and animals, and the same pots and bushes broken
// (soh/soh/Network/Anchor/ActorSync.cpp: "People and animals", "Pots, bushes and rocks").
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/npc-sync-steps.mjs
//
// Both players start a new file (child, daytime) through a join link; A joins first and owns the rooms they share.
// In the Market the walking townspeople must be in the same place on both; while B's Link talks to one it's B's own;
// a bush B cuts is gone on A. At Lon Lon Ranch the wandering horses (which go about at random) must stay together,
// and a pot A breaks is gone on B.

import { startPlayers, syncedList, actors, check, dist } from './sync-test-lib.mjs';

const ENTR_MARKET_DAY = 0xb1;
const ENTR_LON_LON_RANCH = 0x157;
const ACTOR_EN_KUSA = 0x125;
const ACTOR_OBJ_TSUBO = 0x111;
const DMG_SLASH_MASTER = 0x200;

export default async ({ page, sleep, log }) => {
    const { A, B, close } = await startPlayers({ page, sleep }, 'soh-npc-test-');
    const results = {};
    const warp = async (entrance, pos) => {
        for (const p of [A, B]) await p.evaluate((e) => Module._web_debug_warp(e), entrance);
        await sleep(9000);
        if (pos) for (const p of [A, B]) await p.evaluate(([x, y, z]) => Module._web_debug_set_player_pos(x, y, z), pos);
        await sleep(3000);
    };
    // The worst distance between the two players' copies of each of these, over a few seconds
    const follow = async (names, seconds) => {
        const worst = {};
        let samples = 0;
        for (let i = 0; i < seconds * 2; i++) {
            const [la, lb] = [await syncedList(A), await syncedList(B)];
            for (const a of la.filter((s) => names.includes(s.name) && s.fighting)) {
                const b = lb.find((s) => s.name === a.name && s.index === a.index);
                if (!b || !b.fighting) continue;
                const key = `${a.name}@${a.index}`;
                worst[key] = Math.max(worst[key] ?? 0, +dist(a, b).toFixed(1));
                samples++;
            }
            await sleep(500);
        }
        check(samples > 0, `none of ${names} were walking about on both`);
        return worst;
    };
    const propAlive = async (p, id, index) => (await actors(p)).find((a) => a.id === id && a.index === index)?.alive ?? false;
    try {
        // The Market: the townspeople walking their paths
        await warp(ENTR_MARKET_DAY, [0, 0, 300]);
        const market = await follow(['Townsperson', 'Dog'], 12);
        log('market', JSON.stringify(market));
        for (const [key, d] of Object.entries(market)) check(d < 60, `${key} differs by up to ${d}`);
        results.market = market;

        // While B's Link talks to one, it's B's own: B stops taking A's states for it
        const walker = (await syncedList(B)).find((s) => s.name === 'Townsperson' && s.fighting);
        check(walker, 'no townsperson walking on B');
        await B.evaluate(([r, i]) => Module._web_debug_npc_busy(r, i, 1), [walker.room, walker.index]);
        await sleep(2000);
        const busy = (await syncedList(B)).find((s) => s.name === walker.name && s.index === walker.index);
        const appliedBusy = busy.sync?.applied ?? 0;
        await sleep(2000);
        const stillBusy = (await syncedList(B)).find((s) => s.name === walker.name && s.index === walker.index);
        check((stillBusy.sync?.applied ?? 0) === appliedBusy, `B kept taking A's states while talking (${appliedBusy} -> ${stillBusy.sync?.applied})`);
        await B.evaluate(([r, i]) => Module._web_debug_npc_busy(r, i, 0), [walker.room, walker.index]);
        await sleep(2000);
        const after = (await syncedList(B)).find((s) => s.name === walker.name && s.index === walker.index);
        check((after.sync?.applied ?? 0) > appliedBusy, "B didn't follow A's state again after talking");
        results.talk = { applied: [appliedBusy, stillBusy.sync?.applied, after.sync?.applied] };

        // A bush B cuts is gone on A too
        const bush = (await actors(B)).find((a) => a.id === ACTOR_EN_KUSA && a.alive);
        check(bush, 'no bush in the Market');
        check(await propAlive(A, ACTOR_EN_KUSA, bush.index), "A's bush is already gone");
        for (let i = 0; i < 10 && (await propAlive(B, ACTOR_EN_KUSA, bush.index)); i++) {
            await B.evaluate(([r, i, d]) => Module._web_debug_prop_hit(r, i, d), [bush.room, bush.index, DMG_SLASH_MASTER]);
            await sleep(300);
        }
        check(!(await propAlive(B, ACTOR_EN_KUSA, bush.index)), "couldn't cut the bush on B");
        let bushGone = false;
        for (let i = 0; i < 10 && !bushGone; i++) {
            await sleep(300);
            bushGone = !(await propAlive(A, ACTOR_EN_KUSA, bush.index));
        }
        check(bushGone, "the bush B cut is still there on A");
        results.bush = bush.index;

        // Lon Lon Ranch: the horses wander about at random, and must stay together
        await warp(ENTR_LON_LON_RANCH, [600, 0, -500]);
        const ranch = await follow(['Horse'], 15);
        log('ranch', JSON.stringify(ranch));
        for (const [key, d] of Object.entries(ranch)) check(d < 100, `${key} differs by up to ${d}`);
        results.ranch = ranch;

        // A pot A breaks is gone on B
        for (const p of [A, B]) await p.evaluate(() => Module._web_debug_set_player_pos(860, 0, 300));
        await sleep(2000);
        const pot = (await actors(A)).find((a) => a.id === ACTOR_OBJ_TSUBO && a.alive && Math.hypot(a.x - 860, a.z - 200) < 150);
        check(pot, 'no pot near the ranch house');
        for (let i = 0; i < 10 && (await propAlive(A, ACTOR_OBJ_TSUBO, pot.index)); i++) {
            await A.evaluate(([r, i, d]) => Module._web_debug_prop_hit(r, i, d), [pot.room, pot.index, DMG_SLASH_MASTER]);
            await sleep(300);
        }
        check(!(await propAlive(A, ACTOR_OBJ_TSUBO, pot.index)), "couldn't break the pot on A");
        let potGone = false;
        for (let i = 0; i < 10 && !potGone; i++) {
            await sleep(300);
            potGone = !(await propAlive(B, ACTOR_OBJ_TSUBO, pot.index));
        }
        check(potGone, 'the pot A broke is still there on B');
        results.pot = pot.index;
        log('NPCs', JSON.stringify(results));
    } finally {
        await close();
    }
    return results;
};
