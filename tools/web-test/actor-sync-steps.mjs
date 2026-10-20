// Two players in one Anchor room must see the same enemies and push blocks (soh/soh/Network/Anchor/ActorSync.cpp).
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/actor-sync-steps.mjs
//
// Both players load the included save through a join link, so this needs the Anchor server (through the deployed
// bridge) and the game files at OOT_URL. Player A joins first, so it runs the enemies; B follows.

const OOT_URL = process.env.OOT_URL || 'https://share.selst.uk/oot.o2r';

const ENTR_GANONS_TOWER = 0x41b; // room 7: Keese flying around
const ENTR_DEKU_TREE = 0x0; // room 0: Deku Babas and Skulltulas
const ENTR_SPIRIT_TEMPLE = 0x82; // room 0: a push block, spawn list index 3
const ACTOR_OBJ_OSHIHIKI = 0xff;

export default async ({ page, sleep, log }) => {
    const room = 'soh-sync-test-' + Math.floor(Math.random() * 1e6);
    const u = new URL(page.url());
    const link = (name) => `${u.origin}${u.pathname}${u.search}#oot=${OOT_URL}&room=${room}&join=1&slot=1&name=${name}`;
    const ctxB = await page.context().browser().newContext({ viewport: page.viewportSize() });
    const A = page;
    const B = await ctxB.newPage();

    const gameState = (p) => p.evaluate(() => ({ gameState: Module._web_debug_get(13), fileNum: Module._web_debug_get(10) }));
    const actors = async (p) => JSON.parse(await p.evaluate(() => UTF8ToString(Module._web_debug_actor_sync())));
    const warp = async (e) => {
        await A.evaluate((e) => Module._web_debug_warp(e), e);
        await B.evaluate((e) => Module._web_debug_warp(e), e);
        await sleep(8000);
    };
    const waitInGame = async (p, name) => {
        for (let i = 0; i < 90; i++) {
            await sleep(1000);
            try {
                const s = await gameState(p);
                if (s.gameState === 3 && s.fileNum === 0) return;
            } catch (e) {}
        }
        throw new Error(`${name} never got into the game`);
    };
    const key = (a) => `${a.room}:${a.index}`;
    const find = (list, k) => list.find((a) => key(a) === k);
    const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y, a.z - b.z);
    const check = (ok, message) => {
        if (!ok) throw new Error(message);
    };

    try {
        await A.goto('about:blank');
        await A.goto(link('alice'));
        await waitInGame(A, 'A');
        await sleep(2000);
        await B.goto(link('bob'));
        await waitInGame(B, 'B');
        await sleep(4000);
        const results = {};

        // Keese fly a few hundred units in this time; B should stay a few frames behind A
        await warp(ENTR_GANONS_TOWER);
        const keeseStart = await actors(A);
        let worst = 0;
        for (let i = 0; i < 3; i++) {
            await sleep(2000);
            const [la, lb] = [await actors(A), await actors(B)];
            for (const a of la) {
                const b = find(lb, key(a));
                check(b, `keese ${key(a)} missing on B`);
                worst = Math.max(worst, dist(a, b));
            }
        }
        const keeseEnd = await actors(A);
        const moved = Math.max(...keeseStart.map((a) => dist(a, find(keeseEnd, key(a)))));
        log('keese: moved', moved.toFixed(0), 'on A, at most', worst.toFixed(1), 'apart');
        check(moved > 100, `the Keese hardly moved (${moved.toFixed(0)}), so this proves nothing`);
        check(worst < 60, `Keese positions differ by up to ${worst.toFixed(1)}`);
        results.keese = { moved: Math.round(moved), worstDistance: +worst.toFixed(1) };

        // Damage and kills from either player reach the other
        await warp(ENTR_DEKU_TREE);
        const enemies = (await actors(A)).filter((a) => a.alive && a.health > 1);
        check(enemies.length >= 3, `expected 3 enemies with more than 1 health, found ${enemies.length}`);
        const [hurt, killedByB, killedByA] = enemies;
        await B.evaluate(([r, i]) => Module._web_debug_actor_set_health(r, i, 1), [hurt.room, hurt.index]);
        await B.evaluate(([r, i]) => Module._web_debug_actor_defeat(r, i), [killedByB.room, killedByB.index]);
        await A.evaluate(([r, i]) => Module._web_debug_actor_defeat(r, i), [killedByA.room, killedByA.index]);
        await sleep(2000);
        const [la, lb] = [await actors(A), await actors(B)];
        check(find(la, key(hurt))?.health === 1, `damage from B didn't reach A (${find(la, key(hurt))?.health})`);
        for (const [who, list] of [['A', la], ['B', lb]]) {
            for (const killed of [killedByB, killedByA]) {
                const a = find(list, key(killed));
                check(!a || !a.alive, `${key(killed)} is still alive on ${who}`);
            }
        }
        results.enemies = { damaged: key(hurt), killedByB: key(killedByB), killedByA: key(killedByA) };

        // Pushes from either player move the block for both, and someone who comes back later sees it moved
        await warp(ENTR_SPIRIT_TEMPLE);
        const blockPos = async () => {
            const [a, b] = [find(await actors(A), '0:3'), find(await actors(B), '0:3')];
            check(a && b && a.id === ACTOR_OBJ_OSHIHIKI, 'push block 0:3 not found');
            return [a, b];
        };
        const pushes = [];
        const push = async (p, quarter) => {
            const [before] = await blockPos();
            check(await p.evaluate((q) => Module._web_debug_block_push(0, 3, q), quarter), 'push did not start');
            await sleep(3000);
            const [a, b] = await blockPos();
            pushes.push([Math.round(a.x), Math.round(a.y), Math.round(a.z)]);
            check(dist(before, a) > 15, `the block didn't move (${dist(before, a).toFixed(1)})`);
            check(dist(a, b) < 1, `block positions differ by ${dist(a, b).toFixed(1)}`);
        };
        await push(A, 0);
        await push(B, 1);
        await B.evaluate((e) => Module._web_debug_warp(e), ENTR_DEKU_TREE);
        await sleep(6000);
        check(await A.evaluate(() => Module._web_debug_block_push(0, 3, 2)), 'push did not start');
        await sleep(3000);
        await B.evaluate((e) => Module._web_debug_warp(e), ENTR_SPIRIT_TEMPLE);
        await sleep(10000);
        const [a, b] = await blockPos();
        check(dist(a, b) < 1, `B came back to the block ${dist(a, b).toFixed(1)} away from A's`);
        pushes.push([Math.round(a.x), Math.round(a.y), Math.round(a.z)]);
        results.block = pushes;
        return results;
    } finally {
        await ctxB.close();
    }
};
