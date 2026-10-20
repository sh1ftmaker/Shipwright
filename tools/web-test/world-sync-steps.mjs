// Two players in one Anchor room must share world flags, keys and items (soh/soh/Network/Anchor/WorldSync.h).
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/world-sync-steps.mjs
//
// Both players start a new file through a join link, so this needs the Anchor server (through the deployed bridge)
// and the game files at OOT_URL. A joins first and owns the room; B is a guest.

const OOT_URL = process.env.OOT_URL || 'https://share.selst.uk/oot.o2r';

const ENTR_DEKU_TREE = 0x0;
const ENTR_DODONGOS_CAVERN = 0x4;
const ENTR_FOREST_TEMPLE = 0x169;
const SCENE_FIRE_TEMPLE = 4;
const SCENE_GLOBAL = 0x6e; // SCENE_ID_MAX: eventChkInf and the like
const FOREST_TEMPLE_MAP = 3;
const ACTOR_BG_DDAN_KD = 0x5c;
const ACTOR_OBJ_SWITCH = 0x12a;
const ITEM_HEART_PIECE = 0x7a; // ITEM_HEART_PIECE_2, what pickups give
const ITEM_KEY_SMALL = 0x77;
const ITEM_RUPEE_RED = 0x86;
const FLAG = { SWITCH: 1, TREASURE: 2, CLEAR: 3, COLLECTIBLE: 4, EVENT_CHECK_INF: 5, INF_TABLE: 7, EVENT_INF: 8 };
const GET = { GET: 0, SET: 1, UNSET: 2, SET_OFFLINE: 3 };
const INV = { KEYS: 0, HEART_PIECES: 1, MAP_INDEX: 3, RUPEES: 8 };

export default async ({ page, sleep, log }) => {
    const room = 'soh-world-test-' + Math.floor(Math.random() * 1e6);
    const u = new URL(page.url());
    const link = (name) => `${u.origin}${u.pathname}${u.search}#oot=${OOT_URL}&room=${room}&join=1&slot=3&name=${name}`;
    const ctxB = await page.context().browser().newContext({ viewport: page.viewportSize() });
    const A = page;
    const B = await ctxB.newPage();
    B.on('console', (m) => /\[Anchor\]|abort|exception|error/i.test(m.text()) && log('B console:', m.text()));
    B.on('pageerror', (e) => log('B page error:', e.message));

    const gameState = (p) => p.evaluate(() => ({ gameState: Module._web_debug_get(13), fileNum: Module._web_debug_get(10) }));
    const flag = (p, op, scene, type, f) => p.evaluate((a) => Module._web_debug_flag(...a), [op, scene, type, f]);
    const inv = (p, field, index = 0) => p.evaluate((a) => Module._web_debug_inventory(...a), [field, index]);
    const give = (p, item, chest = -1) => p.evaluate((a) => Module._web_debug_give_item(...a), [item, chest]);
    const useKey = (p, door) => p.evaluate((d) => Module._web_debug_use_key(d), door);
    const get = (p, field) => p.evaluate((f) => Module._web_debug_get(f), field);
    const findActor = async (p, id) => {
        const s = await p.evaluate((i) => UTF8ToString(Module._web_debug_find_actor(i)), id);
        if (!s) return null;
        const [params, actorRoom, x, y, z, scaleY] = s.split(' ').map(Number);
        return { params, room: actorRoom, x, y, z, scaleY };
    };
    const both = (fn) => Promise.all([fn(A), fn(B)]);
    const warp = async (e) => {
        await both((p) => p.evaluate((e) => Module._web_debug_warp(e), e));
        await sleep(8000);
    };
    const waitInGame = async (p, name) => {
        for (let i = 0; i < 90; i++) {
            await sleep(1000);
            try {
                const s = await gameState(p);
                if (s.gameState === 3 && s.fileNum === 2) return;
            } catch (e) {}
        }
        throw new Error(`${name} never got into the game`);
    };
    const check = (ok, message) => {
        if (!ok) throw new Error(message);
    };
    // Keys start at -1 (never had one)
    const keys = async (p) => Math.max(0, await inv(p, INV.KEYS, FOREST_TEMPLE_MAP));

    try {
        await A.goto('about:blank');
        await A.goto(link('alice'));
        await waitInGame(A, 'A');
        await sleep(2000);
        await B.goto(link('bob'));
        await waitInGame(B, 'B');
        await sleep(4000);
        const results = {};

        // A guest's switch reaches the host live, and the host's switch actor presses itself
        await warp(ENTR_DEKU_TREE);
        const [x, y, z] = [await get(A, 1), await get(A, 2), await get(A, 3)];
        check(await A.evaluate((a) => Module._web_debug_spawn_actor(...a), [ACTOR_OBJ_SWITCH, 0x1000, x + 80, y, z]), 'switch did not spawn');
        await sleep(1000);
        const switchUp = await findActor(A, ACTOR_OBJ_SWITCH);
        check(switchUp && switchUp.scaleY > 0.1, `switch not up before (${JSON.stringify(switchUp)})`);
        await flag(B, GET.SET, -1, FLAG.SWITCH, 0x10);
        await sleep(2500);
        const switchDown = await findActor(A, ACTOR_OBJ_SWITCH);
        check((await flag(A, GET.GET, -1, FLAG.SWITCH, 0x10)) === 1, "B's switch flag didn't reach A");
        check(switchDown.scaleY < 0.05, `A's switch didn't press itself (scale ${switchDown.scaleY})`);
        results.switch = { before: switchUp.scaleY, after: switchDown.scaleY };

        // Room-local temp flags (0x38-0x3F) stay in their room; scene temp flags reach the whole scene
        await flag(A, GET.SET, -1, FLAG.SWITCH, 0x3c);
        await sleep(2000);
        check((await flag(B, GET.GET, -1, FLAG.SWITCH, 0x3c)) === 1, "a room-local flag didn't reach the same room");
        check(await A.evaluate(() => Module._web_debug_load_room(1)), 'room did not load');
        await sleep(1500);
        check((await get(A, 11)) === 1 && (await get(B, 11)) === 0, 'players are not in rooms 1 and 0');
        await flag(A, GET.SET, -1, FLAG.SWITCH, 0x3a);
        await flag(A, GET.SET, -1, FLAG.SWITCH, 0x25);
        await sleep(2000);
        const roomLocal = await flag(B, GET.GET, -1, FLAG.SWITCH, 0x3a);
        const sceneTemp = await flag(B, GET.GET, -1, FLAG.SWITCH, 0x25);
        check(roomLocal === 0, 'a room-local flag of room 1 leaked into room 0');
        check(sceneTemp === 1, "a scene temp flag didn't reach the other room");
        // Per-player flags stay put
        await flag(A, GET.SET, SCENE_GLOBAL, FLAG.EVENT_INF, 0x5);
        await sleep(2000);
        check((await flag(B, GET.GET, SCENE_GLOBAL, FLAG.EVENT_INF, 0x5)) === 0, 'an eventInf flag was synced');
        results.tempFlags = { roomLocal, sceneTemp };

        // Dodongo's Cavern stairs come down live when a teammate's flag arrives
        await warp(ENTR_DODONGOS_CAVERN);
        let stairs = await findActor(A, ACTOR_BG_DDAN_KD);
        for (let r = 1; !stairs && r < 16; r++) {
            if (!(await A.evaluate((r) => Module._web_debug_load_room(r), r))) continue;
            await sleep(1500);
            stairs = await findActor(A, ACTOR_BG_DDAN_KD);
        }
        check(stairs, 'no Bg_Ddan_Kd in Dodongo Cavern');
        log('stairs in room', stairs.room, 'at y', stairs.y);
        await flag(B, GET.SET, -1, FLAG.SWITCH, stairs.params & 0x3f);
        await sleep(9000);
        const stairsAfter = await findActor(A, ACTOR_BG_DDAN_KD);
        check(stairsAfter.y < stairs.y - 100, `the stairs didn't come down on A (${stairs.y} -> ${stairsAfter.y})`);
        results.stairs = { from: stairs.y, to: stairsAfter.y };

        // Small keys
        await warp(ENTR_FOREST_TEMPLE);
        check((await inv(A, INV.MAP_INDEX)) === FOREST_TEMPLE_MAP, 'not in the Forest Temple');
        const keyLog = [];
        const keyCounts = async (what, expected) => {
            await sleep(3000);
            const [ka, kb] = [await keys(A), await keys(B)];
            keyLog.push([what, ka, kb]);
            check(ka === expected && kb === expected, `${what}: keys are A ${ka}, B ${kb}, expected ${expected}`);
        };
        // Both pick up a different key at the same moment
        await Promise.all([give(A, ITEM_KEY_SMALL, 10), give(B, ITEM_KEY_SMALL, 11)]);
        await keyCounts('two pickups at once', 2);
        // Both unlock the same door at the same moment: one key
        await Promise.all([useKey(A, 0x15), useKey(B, 0x15)]);
        await keyCounts('same door at once', 1);
        // Both take the key from the same chest at the same moment: one key
        await Promise.all([give(A, ITEM_KEY_SMALL, 12), give(B, ITEM_KEY_SMALL, 12)]);
        await keyCounts('same chest at once', 2);
        // Different doors at once
        await Promise.all([useKey(A, 0x16), useKey(B, 0x17)]);
        await keyCounts('two doors at once', 0);
        results.keys = keyLog;

        // Chest race: one heart piece each, not two
        const pieces = async () => [await inv(A, INV.HEART_PIECES), await inv(B, INV.HEART_PIECES)];
        const [pa, pb] = await pieces();
        await Promise.all([give(A, ITEM_HEART_PIECE, 13), give(B, ITEM_HEART_PIECE, 13)]);
        await sleep(3000);
        const [pa2, pb2] = await pieces();
        check(pa2 === (pa + 1) % 4 && pb2 === (pb + 1) % 4, `chest race: heart pieces ${pa}->${pa2} and ${pb}->${pb2}`);
        // Without a race the teammate gets it, and the chest opens for them
        await give(A, ITEM_HEART_PIECE, 14);
        await sleep(3000);
        const [pa3, pb3] = await pieces();
        check(pa3 === (pa2 + 1) % 4 && pb3 === (pb2 + 1) % 4, `heart piece: ${pa2}->${pa3} and ${pb2}->${pb3}`);
        check((await flag(B, GET.GET, -1, FLAG.TREASURE, 14)) === 1, "A's chest didn't open for B");
        results.heartPieces = [[pa, pa2, pa3], [pb, pb2, pb3]];

        // Consumables stay with whoever picked them up
        const rupeesB = await inv(B, INV.RUPEES);
        await give(A, ITEM_RUPEE_RED);
        await sleep(2000);
        check((await inv(B, INV.RUPEES)) === rupeesB, "A's rupees reached B");

        // Team state merge: progress each made without the other (no packets) survives, and reaches both
        await flag(A, GET.SET_OFFLINE, SCENE_FIRE_TEMPLE, FLAG.TREASURE, 6);
        await flag(B, GET.SET_OFFLINE, SCENE_FIRE_TEMPLE, FLAG.TREASURE, 5);
        check(await B.evaluate(() => Module._web_debug_anchor_request_team_state()), 'B is not connected');
        await sleep(6000);
        const merged = {
            A: [await flag(A, GET.GET, SCENE_FIRE_TEMPLE, FLAG.TREASURE, 5), await flag(A, GET.GET, SCENE_FIRE_TEMPLE, FLAG.TREASURE, 6)],
            B: [await flag(B, GET.GET, SCENE_FIRE_TEMPLE, FLAG.TREASURE, 5), await flag(B, GET.GET, SCENE_FIRE_TEMPLE, FLAG.TREASURE, 6)],
        };
        check(merged.B[0] === 1, "B's own progress was rolled back by A's team state");
        check(merged.B[1] === 1, "A's progress didn't reach B through the team state");
        check(merged.A[0] === 1, "B's progress didn't reach A after the merge");
        check(merged.A[1] === 1, "A lost its own progress");
        // ... and the merge kept everyone's keys and heart pieces
        check((await keys(B)) === 0 && (await inv(B, INV.HEART_PIECES)) === pb3, 'the merge changed keys or heart pieces');
        results.merge = merged;
        return results;
    } finally {
        await ctxB.close();
    }
};
