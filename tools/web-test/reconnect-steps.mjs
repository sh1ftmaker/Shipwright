// Anchor connections must survive drops: reconnecting, save changes made while offline, room ownership, reloads and
// hidden tabs (soh/soh/Network/Anchor/Anchor.cpp, AnchorWeb.cpp, the bridge's keepalive timeout).
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/reconnect-steps.mjs
//
// Two players join one room through join links, so this needs the Anchor server (through the deployed bridge) and
// the game files at OOT_URL. Player B's WebSocket goes through page.routeWebSocket, so the test can cut it cleanly
// or silently (packets stop flowing but nothing closes, like a wifi drop).

const OOT_URL = process.env.OOT_URL || 'https://share.selst.uk/oot.o2r';

// Event flags a new file doesn't have, and items it doesn't have (GI_* to give, ITEM_* to look for)
const FLAG_HALF_OPEN = 0xd5;
const FLAG_OFFLINE = 0xd6;
const ITEM_BOOMERANG = [0x06, 0x0e];
const ITEM_LENS = [0x0a, 0x0f];

export default async ({ page, sleep, log }) => {
    const room = 'soh-reconnect-test-' + Math.floor(Math.random() * 1e6);
    const u = new URL(page.url());
    const link = (name) => `${u.origin}${u.pathname}${u.search}#oot=${OOT_URL}&room=${room}&join=1&slot=3&name=${name}`;
    const ctxB = await page.context().browser().newContext({ viewport: page.viewportSize() });
    const A = page;
    const B = await ctxB.newPage();
    B.on('console', (m) => {
        if (/\[(Anchor|Network|WebSocket)\]/.test(m.text())) log('B:', m.text().slice(0, 160));
    });

    // B's connection to the bridge. "silent" stops messages both ways without closing anything; "refuse" closes new
    // connections as soon as they open.
    const net = { silent: false, refuse: false, route: null, connections: 0 };
    await B.routeWebSocket(
        (url) => url.href.includes('anchor-bridge'),
        (ws) => {
            net.connections++;
            if (net.refuse) {
                ws.close({ code: 4001, reason: 'test: refused' });
                return;
            }
            const server = ws.connectToServer();
            const route = (net.route = { ws, server, silent: false });
            ws.onMessage((m) => !route.silent && server.send(m));
            server.onMessage((m) => !route.silent && ws.send(m));
            // A silent connection's server side stays open, as it would when the network just vanishes
            ws.onClose(() => !route.silent && server.close());
            server.onClose((code, reason) => !route.silent && ws.close({ code, reason }));
        },
    );

    const anchor = async (p) => JSON.parse(await p.evaluate(() => UTF8ToString(Module._web_debug_anchor())));
    // Calling into the game before its runtime is up aborts it (ASSERTIONS), so check first
    const gameState = (p) =>
        p.evaluate(() => (typeof Module !== 'undefined' && Module.calledRun
            ? { gameState: Module._web_debug_get(13), fileNum: Module._web_debug_get(10) } : {}));
    const hasFlag = (p, f) => p.evaluate((f) => Module._web_debug_get_event_flag(f), f);
    const hasItem = (p, [, item]) => p.evaluate((i) => Module._web_debug_has_item(i), item);
    const give = (p, [gi]) => p.evaluate((g) => Module._web_debug_give_item(g), gi);
    const setFlag = (p, f) => p.evaluate((f) => Module._web_debug_set_event_flag(f), f);
    const setHidden = (p, hidden) =>
        p.evaluate((h) => {
            Object.defineProperty(document, 'visibilityState', { configurable: true, get: () => (h ? 'hidden' : 'visible') });
            Object.defineProperty(document, 'hidden', { configurable: true, get: () => h });
            document.dispatchEvent(new Event('visibilitychange'));
        }, hidden);
    const check = (ok, message) => {
        if (!ok) throw new Error(message);
    };
    const waitFor = async (what, fn, seconds) => {
        const start = Date.now();
        let last;
        while (Date.now() - start < seconds * 1000) {
            try {
                last = await fn();
                if (last) return (Date.now() - start) / 1000;
            } catch (e) {
                last = String(e);
            }
            await sleep(250);
        }
        throw new Error(`timed out after ${seconds}s waiting for ${what} (last: ${JSON.stringify(last)})`);
    };
    const waitInGame = (p, name) =>
        waitFor(`${name} in the game`, async () => {
            const s = await gameState(p);
            return s.gameState === 3 && s.fileNum === 2;
        }, 90);
    const client = (state, id) => state.clients.find((c) => c.clientId === id);

    try {
        await A.goto('about:blank');
        await A.goto(link('alice'));
        await waitInGame(A, 'A');
        await sleep(2000);
        await B.goto(link('bob'));
        await waitInGame(B, 'B');
        await waitFor('both connected and seeing each other', async () => {
            const [a, b] = [await anchor(A), await anchor(B)];
            return a.sessionReady && b.sessionReady && client(a, b.clientId)?.online && client(b, a.clientId)?.online;
        }, 30);
        const idA = (await anchor(A)).clientId;
        const idB = (await anchor(B)).clientId;
        check((await anchor(B)).ownerClientId === idA, 'A made the room but does not own it');
        for (const [p, f] of [[A, FLAG_HALF_OPEN], [A, FLAG_OFFLINE], [B, FLAG_HALF_OPEN], [B, FLAG_OFFLINE]]) {
            check(!(await hasFlag(p, f)), `flag ${f} is already set on a new file`);
        }
        for (const item of [ITEM_BOOMERANG, ITEM_LENS]) check(!(await hasItem(A, item)), `A already has item ${item[1]}`);
        log('ids: A', idA, 'B', idB);
        const results = { ids: [idA, idB] };

        // A clean drop (the bridge or server closes the socket): back within a couple of seconds, same ID
        net.route.ws.close({ code: 1011, reason: 'test: dropped' });
        await waitFor('B to notice the drop', async () => !(await anchor(B)).sessionReady, 5);
        const cleanSecs = await waitFor('B to reconnect', async () => (await anchor(B)).sessionReady, 15);
        check((await anchor(B)).clientId === idB, 'B came back with another ID');
        await waitFor('A to see B online again', async () => client(await anchor(A), idB)?.online, 10);
        log('clean drop: back after', cleanSecs.toFixed(1), 's');
        results.cleanDropSecs = +cleanSecs.toFixed(1);

        // A silent drop: nothing closes, packets just stop. A flag and an item picked up right after go into the dead
        // connection; B must notice (keepalive) and send them again. Then, while B can't connect at all, another flag
        // and item; they go out once it can.
        net.route.silent = true;
        net.refuse = true;
        await setFlag(B, FLAG_HALF_OPEN);
        await give(B, ITEM_BOOMERANG);
        const detectSecs = await waitFor('B to notice the dead connection', async () => !(await anchor(B)).connected, 20);
        log('silent drop: noticed after', detectSecs.toFixed(1), 's');
        await setFlag(B, FLAG_OFFLINE);
        await give(B, ITEM_LENS);
        await sleep(3000);
        check(!(await hasFlag(A, FLAG_OFFLINE)) && !(await hasItem(A, ITEM_LENS)), 'A got B\'s offline changes while B was offline');
        const offlineB = client(await anchor(A), idB);
        log('A sees B as', JSON.stringify(offlineB), 'while B is cut off');
        net.refuse = false;
        const silentSecs = await waitFor('B to reconnect', async () => (await anchor(B)).sessionReady, 30);
        check((await anchor(B)).clientId === idB, 'B came back with another ID');
        const syncSecs = await waitFor('A to get everything B did while cut off', async () =>
            (await hasFlag(A, FLAG_HALF_OPEN)) && (await hasFlag(A, FLAG_OFFLINE)) &&
            (await hasItem(A, ITEM_BOOMERANG)) && (await hasItem(A, ITEM_LENS)), 30);
        log('silent drop: reconnected after', silentSecs.toFixed(1), 's, changes on A', syncSecs.toFixed(1), 's later');
        results.silentDrop = { detectSecs: +detectSecs.toFixed(1), reconnectSecs: +silentSecs.toFixed(1), syncSecs: +syncSecs.toFixed(1) };

        // B's tab hidden: A sees B as away (no save loaded) and back again
        await setHidden(B, true);
        const awaySecs = await waitFor('A to see B away', async () => {
            const b = client(await anchor(A), idB);
            return b?.online && b.paused && !b.isSaveLoaded;
        }, 10);
        await sleep(2000);
        await setHidden(B, false);
        const backSecs = await waitFor('A to see B back', async () => {
            const b = client(await anchor(A), idB);
            return b?.online && !b.paused && b.isSaveLoaded;
        }, 10);
        check((await anchor(B)).sessionReady, 'B lost its connection over being hidden');
        results.hiddenTab = { awaySecs: +awaySecs.toFixed(1), backSecs: +backSecs.toFixed(1) };

        // The owner's tab hidden: B takes the room over, and keeps it when A comes back
        await setHidden(A, true);
        const handoverSecs = await waitFor('B to own the room', async () =>
            (await anchor(B)).ownerClientId === idB && (await anchor(A)).ownerClientId === idB, 20);
        await setHidden(A, false);
        await sleep(3000);
        check((await anchor(A)).ownerClientId === idB, 'A took the room back');
        log('owner hidden: B owns the room after', handoverSecs.toFixed(1), 's');
        results.hiddenOwnerHandoverSecs = +handoverSecs.toFixed(1);

        // A reload keeps the tab's ID, without leaving a ghost behind. Loading the game takes longer than the owner may
        // be away, so A takes the room over meanwhile.
        await B.reload();
        await waitInGame(B, 'B after reloading');
        await waitFor('B connected after reloading', async () => (await anchor(B)).sessionReady, 30);
        const afterReload = await anchor(B);
        check(afterReload.clientId === idB, `B got ID ${afterReload.clientId} after reloading, had ${idB}`);
        const bobs = (await anchor(A)).clients.filter((c) => c.name === 'bob');
        check(bobs.length === 1, `A sees ${bobs.length} entries named bob`);
        await waitFor('A and B to agree on the owner', async () =>
            (await anchor(A)).ownerClientId === (await anchor(B)).ownerClientId, 10);
        results.reloadKeptId = idB;

        // The owner leaves (A by leaving the page, B by closing its tab): the other one takes the room over
        const ownerBefore = (await anchor(B)).ownerClientId;
        const [leaver, stayer, stayerId] = ownerBefore === idA ? [A, B, idB] : [B, A, idA];
        log('owner', ownerBefore, 'leaves');
        if (leaver === A) await A.goto('about:blank');
        else await B.close();
        const leaveSecs = await waitFor('the other player to own the room', async () =>
            (await anchor(stayer)).ownerClientId === stayerId, 20);
        log('owner left: new owner after', leaveSecs.toFixed(1), 's');
        results.ownerLeftHandoverSecs = +leaveSecs.toFixed(1);
        results.bridgeConnectionsB = net.connections;
        return results;
    } finally {
        await ctxB.close();
    }
};
