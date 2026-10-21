// Helpers shared by the two-player Anchor sync tests (actor-sync-steps.mjs, boss-sync-steps.mjs)

export const OOT_URL = process.env.OOT_URL || 'https://share.selst.uk/oot.o2r';

export const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y, a.z - b.z);

export const check = (ok, message) => {
    if (!ok) throw new Error(message);
};

// A join link for one player into an Anchor room, on a new file in slot 3 (or a new Boss Rush run). `base` is the
// page's URL (a page, or its URL from before it left the game).
export const joinLink = (base, room, name, { bossRush = false } = {}) => {
    const u = new URL(typeof base === 'string' ? base : base.url());
    return `${u.origin}${u.pathname}${u.search}#oot=${OOT_URL}&room=${room}&join=1&name=${name}` +
        (bossRush ? '&bossrush=1' : '&slot=3');
};

// Boss Rush plays from no save slot
export const BOSS_RUSH_FILE = 0xfe;

export async function waitInGame(p, name, sleep, fileNum = 2) {
    for (let i = 0; i < 90; i++) {
        await sleep(1000);
        try {
            // Calling into the game before its runtime is up aborts it (ASSERTIONS), so check first
            const s = await p.evaluate(() => (typeof Module !== 'undefined' && Module.calledRun
                ? { gameState: Module._web_debug_get(13), fileNum: Module._web_debug_get(10) } : {}));
            if (s.gameState === 3 && s.fileNum === fileNum) return;
        } catch (e) {}
    }
    throw new Error(`${name} never got into the game`);
}

// Two players in a fresh Anchor room, each on a new file (slot 3, or Boss Rush) through a join link. A joins first,
// so it owns the rooms it shares with B and runs their enemies and bosses.
export async function startPlayers({ page, sleep }, roomPrefix, { bossRush = false } = {}) {
    const room = roomPrefix + Math.floor(Math.random() * 1e6);
    const base = page.url();
    const link = (name) => joinLink(base, room, name, { bossRush });
    const ctxB = await page.context().browser().newContext({ viewport: page.viewportSize() });
    const A = page;
    const B = await ctxB.newPage();
    const fileNum = bossRush ? BOSS_RUSH_FILE : 2;
    await A.goto('about:blank');
    await A.goto(link('alice'));
    await waitInGame(A, 'A', sleep, fileNum);
    await sleep(2000);
    await B.goto(link('bob'));
    await waitInGame(B, 'B', sleep, fileNum);
    await sleep(4000);
    return { A, B, link, room, base, close: () => ctxB.close() };
}

export const actors = async (p) => JSON.parse(await p.evaluate(() => UTF8ToString(Module._web_debug_actor_sync())));

// State-synced fights in the current scene (see web_debug_synced in ActorSync.cpp)
export const syncedList = async (p) => JSON.parse(await p.evaluate(() => UTF8ToString(Module._web_debug_synced())));
// "name" for the first one, "name@index" for the one with that spawn list index
export const synced = async (p, name) => {
    const [defName, index] = name.split('@');
    return (await syncedList(p)).find((s) => s.name === defName && (index === undefined || s.index === +index)) ?? null;
};

export const syncedHit = (p, name, member, part, dmgFlags) =>
    p.evaluate(
        (args) => Module.ccall('web_debug_synced_hit', 'number', ['string', 'number', 'number', 'number'], args),
        [name, member, part, dmgFlags]
    );
export const syncedDefeat = (p, name) =>
    p.evaluate((n) => Module.ccall('web_debug_synced_defeat', 'number', ['string'], [n]), name);
export const syncedSetHealth = (p, name, member, health) =>
    p.evaluate(
        (args) => Module.ccall('web_debug_synced_set_health', 'number', ['string', 'number', 'number'], args),
        [name, member, health]
    );

export const setCVar = (p, name, value) =>
    p.evaluate((args) => Module.ccall('web_cvar_set_int', null, ['string', 'number'], args), [name, value]);
// Something only the player who does it sees, like King Dodongo swallowing their bomb (sDebugPokes in ActorSync.cpp)
export const syncedPoke = (p, name, member) =>
    p.evaluate((args) => Module.ccall('web_debug_synced_poke', 'number', ['string', 'number'], args), [name, member]);

export const anchorState = async (p) => JSON.parse(await p.evaluate(() => UTF8ToString(Module._web_debug_anchor())));
