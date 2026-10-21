// Two players on Boss Rush in one Anchor room (join links with bossrush=1), and a third on a normal file who must
// not share items and flags with them.
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/bossrush-sync-steps.mjs
//
// Checks that both start the same new run in the Chamber of Sages, see each other, fight the same Gohma (B's defeat
// starts A's death), and that a boss beaten by one of them alone counts in the other's Chamber of Sages too. Then C
// joins on a normal file: the team state they share isn't merged into C's file, and C's flags don't reach A.

import { startPlayers, joinLink, waitInGame, anchorState, synced, syncedDefeat, setCVar, check } from './sync-test-lib.mjs';

const SCENE_GLOBAL = 0x6e; // SCENE_ID_MAX: eventChkInf and the like
const SCENE_CHAMBER_OF_THE_SAGES = 0x44;
const ENTR_DEKU_TREE_BOSS = 0x40f;
const ENTR_DODONGOS_CAVERN_BOSS = 0x40b;
const ENTR_CHAMBER_OF_THE_SAGES = 0x6b;
const QUEST_BOSSRUSH = 3;
const EVENTCHKINF_USED_DEKU_TREE_BLUE_WARP = 0x09;
const EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP = 0x25;
const EVENTCHKINF_BEGAN_KING_DODONGO_BATTLE = 0x71;
const EVENTCHKINF_SOME_NORMAL_FLAG = 0x0f; // Anything a Boss Rush file never sets on its own
const FLAG = { EVENT_CHECK_INF: 5 };
const GET = { GET: 0, SET: 1 };

export default async ({ page, sleep, log }) => {
    const { A, B, room, base, close } = await startPlayers({ page, sleep }, 'soh-bossrush-test-', { bossRush: true });
    const extra = [];
    const results = {};
    const flag = (p, op, f) => p.evaluate((a) => Module._web_debug_flag(...a), [op, SCENE_GLOBAL, FLAG.EVENT_CHECK_INF, f]);
    const scene = (p) => p.evaluate(() => Module._web_debug_get(0));
    const warp = async (players, entrance) => {
        for (const p of players) await p.evaluate((e) => Module._web_debug_warp(e), entrance);
        await sleep(9000);
    };
    const until = async (what, seconds, done) => {
        for (let i = 0; i < seconds * 2; i++) {
            const value = await done();
            if (value) return value;
            await sleep(500);
        }
        throw new Error(`couldn't ${what}`);
    };
    try {
        // The same new run on both, and each sees the other
        for (const [p, name] of [[A, 'A'], [B, 'B']]) {
            const s = await anchorState(p);
            check(s.questId === QUEST_BOSSRUSH, `${name} isn't on Boss Rush (${s.questId})`);
            check(s.clients.filter((c) => !c.self && c.isSaveLoaded && c.questId === QUEST_BOSSRUSH).length === 1,
                `${name} doesn't see the other player in Boss Rush (${JSON.stringify(s.clients)})`);
            check(s.questMismatches === 0, `${name} saw ${s.questMismatches} packets from another kind of file`);
        }
        results.scenes = [await scene(A), await scene(B)];
        check(results.scenes.every((s) => s === SCENE_CHAMBER_OF_THE_SAGES), `not both in the Chamber of Sages (${results.scenes})`);

        // Gohma together: B's defeat starts A's death, and both count her as beaten
        for (const p of [A, B]) await setCVar(p, 'gCheats.InfiniteHealth', 1);
        await warp([A, B], ENTR_DEKU_TREE_BOSS);
        for (const p of [A, B]) await p.evaluate(() => Module._web_debug_set_player_pos(150, -480, 350)); // her intro
        await until('get Gohma fighting on both', 60, async () => (await synced(A, 'Gohma'))?.fighting && (await synced(B, 'Gohma'))?.fighting);
        await until('defeat Gohma on B', 20, () => syncedDefeat(B, 'Gohma'));
        await until("see B's defeat on A", 10, async () => {
            const s = await synced(A, 'Gohma');
            return !s || s.defeated;
        });
        await until('count Gohma as beaten on both', 30, async () =>
            (await flag(A, GET.GET, EVENTCHKINF_USED_DEKU_TREE_BLUE_WARP)) && (await flag(B, GET.GET, EVENTCHKINF_USED_DEKU_TREE_BLUE_WARP)));
        results.gohma = true;

        // King Dodongo by A alone, while B waits in the Chamber of Sages: it counts for B too
        await warp([B], ENTR_CHAMBER_OF_THE_SAGES);
        await A.evaluate((f) => Module._web_debug_set_event_flag(f), EVENTCHKINF_BEGAN_KING_DODONGO_BATTLE);
        await warp([A], ENTR_DODONGOS_CAVERN_BOSS);
        await A.evaluate(() => Module._web_debug_set_player_pos(-890, -1523, -3304)); // dropping in starts his intro
        await until('get King Dodongo fighting on A', 60, async () => (await synced(A, 'King Dodongo'))?.fighting);
        await until('defeat King Dodongo on A', 20, () => syncedDefeat(A, 'King Dodongo'));
        // His death cutscene counts him
        await until('count King Dodongo as beaten on A', 90, () => flag(A, GET.GET, EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP));
        await until('see King Dodongo counted on B', 10, () => flag(B, GET.GET, EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP));
        check(await scene(B) === SCENE_CHAMBER_OF_THE_SAGES, 'B left the Chamber of Sages');
        results.kingDodongo = true;

        // C on a normal file: nothing of the Boss Rush run gets into it, and nothing of its file into theirs
        const ctxC = await page.context().browser().newContext({ viewport: page.viewportSize() });
        extra.push(ctxC);
        const C = await ctxC.newPage();
        await C.goto(joinLink(base, room, 'carol'));
        await waitInGame(C, 'C', sleep);
        await sleep(8000);
        const c = await anchorState(C);
        check(c.questId === 0, `C isn't on a normal file (${c.questId})`);
        check(c.questMismatches > 0, "C didn't notice the team's save is Boss Rush");
        check(!(await flag(C, GET.GET, EVENTCHKINF_USED_DEKU_TREE_BLUE_WARP)), "Boss Rush progress got into C's normal file");
        const before = (await anchorState(A)).questMismatches;
        await flag(C, GET.SET, EVENTCHKINF_SOME_NORMAL_FLAG);
        await sleep(3000);
        check(!(await flag(A, GET.GET, EVENTCHKINF_SOME_NORMAL_FLAG)), "C's flag got into A's Boss Rush run");
        const after = (await anchorState(A)).questMismatches;
        check(after > before, "A didn't notice C's flag came from another kind of file");
        results.otherQuest = { c: c.questMismatches, a: after - before };
        log('Boss Rush', JSON.stringify(results));
    } finally {
        for (const ctx of extra) await ctx.close();
        await close();
    }
    return results;
};
