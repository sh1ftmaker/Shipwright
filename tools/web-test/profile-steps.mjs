// A link without the player's name (a quick invite: #oot=...&room=abc&join=1) asks for a profile before the game
// starts (soh/soh/web/shell.html, Profile).
//
//   node tools/web-test/gameplay-test.mjs --rom <rom.z64> --included-save --steps tools/web-test/profile-steps.mjs
//
// Joins an Anchor room, so this needs the Anchor server (through the deployed bridge) and the game files at OOT_URL.

import { OOT_URL, check } from './sync-test-lib.mjs';

export default async ({ page, sleep, log }) => {
    const u = new URL(page.url());
    const base = `${u.origin}${u.pathname}${u.search}`;
    const room = 'soh-profile-test-' + Math.floor(Math.random() * 1e6);
    // A browser that has never been here
    const ctx = await page.context().browser().newContext({ viewport: page.viewportSize() });
    const P = await ctx.newPage();
    P.on('console', (m) => m.type() === 'error' && log('P:', m.text().slice(0, 200)));
    P.on('pageerror', (e) => log('P pageerror:', e.message));
    const open = () => P.evaluate(() => document.getElementById('profile').classList.contains('open'));

    try {
        const results = {};
        await P.goto(`${base}#oot=${OOT_URL}&room=${room}&join=1`);
        await P.waitForSelector('#profile.open', { timeout: 30000 });
        const form = await P.evaluate(() => ({
            room: document.getElementById('profile-room').value,
            roomFixed: document.getElementById('profile-room').readOnly,
            slot: document.getElementById('profile-slot').value,
            invite: document.getElementById('profile-invite').value,
        }));
        log('dialog', JSON.stringify(form));
        check(form.room === room && form.roomFixed, "the link's room should be shown and fixed");
        check(form.slot === '3', 'a join link should load file 3 unless the player picks another');
        check(form.invite.endsWith(`#oot=${OOT_URL}&room=${room}&join=1`), `wrong invite link ${form.invite}`);

        await P.fill('#profile-name', 'carol');
        // The color picker: drag on the hue strip and the square, then type a code (the one kept below)
        const hex = () => P.inputValue('#profile-hex');
        await P.click('.swatch[data-color="ff4040"]');
        check((await hex()) === '#FF4040', `a swatch should show its code (${await hex()})`);
        const hue = await P.locator('#profile-hue').boundingBox();
        await P.mouse.move(hue.x + 2, hue.y + hue.height / 2);
        await P.mouse.down();
        await P.mouse.move(hue.x + hue.width * 2 / 3, hue.y + hue.height / 2, { steps: 5 });
        await P.mouse.up();
        const sv = await P.locator('#profile-sv').boundingBox();
        await P.mouse.click(sv.x + sv.width - 2, sv.y + 2);
        const dragged = (await hex()).slice(1).match(/../g).map((c) => parseInt(c, 16));
        results.dragged = await hex();
        check(dragged[2] > 230 && dragged[0] < 30 && dragged[1] < 30, `dragging to blue gave ${results.dragged}`);
        await P.fill('#profile-hex', '#ff4040');
        check(await P.evaluate(() => document.querySelector('.swatch[data-color="ff4040"]').classList.contains('on')),
            "typing a swatch's code should select that swatch");
        await P.click('#profile button.play');
        for (let i = 0; ; i++) {
            check(i < 90, 'never got into the game');
            await sleep(1000);
            const ok = await P.evaluate(() => typeof Module !== 'undefined' && Module.calledRun &&
                Module._web_debug_get(13) === 3 && Module._web_debug_get(10) === 2).catch(() => false);
            if (ok) break;
        }
        await sleep(3000);
        const me = JSON.parse(await P.evaluate(() => UTF8ToString(Module._web_debug_anchor())));
        const self = me.clients.find((c) => c.self);
        log('anchor', JSON.stringify({ connected: me.connected, self }));
        check(me.connected && self?.name === 'carol', `should be in the room as carol (${self?.name})`);
        results.name = self.name;

        // A reload of the same link doesn't ask again, nor does a plain visit once a profile is picked
        await P.reload();
        await sleep(4000);
        check(!(await open()), 'a reload asked for the profile again');
        await P.goto(base);
        await sleep(4000);
        check(!(await open()), 'a plain visit with a saved profile asked again');
        results.saved = JSON.parse(await P.evaluate(() => localStorage.getItem('soh-profile')));
        check(results.saved.color === 'ff4040', `the color wasn't kept (${results.saved.color})`);
        return results;
    } finally {
        await ctx.close();
    }
};
