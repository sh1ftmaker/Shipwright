// Players can leave and come back in any order: whoever stays takes over the Anchor room and the enemies of the map
// room they're in, and someone who comes back follows them (soh/soh/Network/Anchor/Anchor.cpp CheckRoomOwner,
// ActorSync.cpp RoomOwner).
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/handover-steps.mjs
//
// Two players join one room through join links, so this needs the Anchor server (through the deployed bridge) and
// the game files at OOT_URL.

import { actors, check, dist, startPlayers } from './sync-test-lib.mjs';

const ENTR_GANONS_TOWER = 0x41b; // Keese flying around
const ENTR_DEKU_TREE = 0x0;

export default async ({ page, sleep, log }) => {
    const { A, B, link, close } = await startPlayers({ page, sleep }, 'soh-handover-test-');
    const anchor = async (p) => JSON.parse(await p.evaluate(() => UTF8ToString(Module._web_debug_anchor())));
    const roomOwner = (p, room) => p.evaluate((r) => Module._web_debug_room_owner(r), room);
    const waitFor = async (what, done, secs) => {
        const start = Date.now();
        for (;;) {
            try {
                if (await done()) return (Date.now() - start) / 1000;
            } catch (e) {}
            if (Date.now() - start > secs * 1000) throw new Error(`timed out waiting for ${what}`);
            await sleep(500);
        }
    };
    const inGame = (p) =>
        p.evaluate(() => typeof Module !== 'undefined' && Module.calledRun &&
            Module._web_debug_get(13) === 3 && Module._web_debug_get(10) === 2);
    const warp = async (p, e) => {
        await p.evaluate((e) => Module._web_debug_warp(e), e);
        await sleep(8000);
    };
    // Both run the room's enemies; whoever doesn't own it follows, so they stay close
    const keeseApart = async () => {
        let worst = 0;
        for (let i = 0; i < 3; i++) {
            await sleep(1500);
            const [la, lb] = [await actors(A), await actors(B)];
            for (const a of la) {
                const b = lb.find((x) => x.room === a.room && x.index === a.index);
                if (b && a.alive && b.alive) worst = Math.max(worst, dist(a, b));
            }
        }
        return +worst.toFixed(1);
    };
    const owners = async (room) => [await roomOwner(A, room), await roomOwner(B, room)];

    try {
        const results = {};
        const [idA, idB] = [(await anchor(A)).clientId, (await anchor(B)).clientId];
        log('A is', idA, 'B is', idB);
        check(idA < idB, 'A joined first, so it should have the lower client ID');

        // Arriving together: the lower client ID runs the room
        await Promise.all([warp(A, ENTR_GANONS_TOWER), warp(B, ENTR_GANONS_TOWER)]);
        const room = (await actors(A))[0]?.room;
        check(room !== undefined, 'no Keese here');
        check((await owners(room)).every((o) => o === idA), `together: A should run the room (${await owners(room)})`);

        // A leaves: B takes over the Anchor room and the enemies
        await A.goto('about:blank');
        results.adminHandoverSecs = +(await waitFor('B to own the Anchor room', async () =>
            (await anchor(B)).ownerClientId === idB, 20)).toFixed(1);
        check((await roomOwner(B, room)) === idB, 'B should run the enemies once A is gone');

        // A comes back through the same link, in the same tab, so with its old (lower) client ID: B stays in charge
        await A.goto(link('alice'));
        await waitFor('A back in the game', () => inGame(A), 90);
        await sleep(4000);
        check((await anchor(A)).clientId === idA, 'A should get its client ID back');
        await warp(A, ENTR_GANONS_TOWER);
        check((await anchor(A)).ownerClientId === idB, 'B should stay the Anchor room owner after A is back');
        check((await owners(room)).every((o) => o === idB), `after A came back, B should run the room (${await owners(room)})`);
        results.backApart = await keeseApart();
        check(results.backApart < 60, `A came back to Keese up to ${results.backApart} away from B's`);

        // Now B steps out of the room and back: A has been there longest, so A runs it and B follows
        await warp(B, ENTR_DEKU_TREE);
        check((await roomOwner(A, room)) === idA, 'A should run the room while B is away');
        await warp(B, ENTR_GANONS_TOWER);
        check((await owners(room)).every((o) => o === idA), `after B came back, A should run the room (${await owners(room)})`);
        results.swapApart = await keeseApart();
        check(results.swapApart < 60, `B came back to Keese up to ${results.swapApart} away from A's`);
        return results;
    } finally {
        await close();
    }
};
