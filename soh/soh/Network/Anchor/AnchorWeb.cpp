#ifdef __EMSCRIPTEN__

#include "Anchor.h"
#include <emscripten.h>

// MARK: - Client ID kept per tab

// The ID lives in sessionStorage, which survives reloads but belongs to one tab. Duplicating a tab copies
// sessionStorage, so the ID is only used while this page holds a Web Lock named after it: the tab it came from holds
// it, and the duplicate starts fresh. A reloading page lets go of the lock as it unloads, which can take a moment.
EM_JS(void, anchor_web_claim_client_id, (), {
    if (Module.anchorClientId) {
        return;
    }
    var state = Module.anchorClientId = { id : 0, held : false, release : null };
    var stored = 0;
    try {
        stored = parseInt(sessionStorage.getItem('sohAnchorClientId') || '0', 10) || 0;
    } catch (e) {
    }
    if (!stored || !navigator.locks) {
        return;
    }
    var controller = new AbortController();
    setTimeout(function() { controller.abort(); }, 1500);
    navigator.locks
        .request('soh-anchor-client-' + stored, { signal : controller.signal },
                 function(lock) {
                     if (state.id) {
                         return; // The server gave us another ID meanwhile
                     }
                     state.id = stored;
                     state.held = true;
                     return new Promise(function(resolve) { state.release = resolve; });
                 })
        .catch(function() {});
});

EM_JS(int, anchor_web_stored_client_id, (), {
    var state = Module.anchorClientId;
    return state && state.held ? state.id : 0;
});

EM_JS(void, anchor_web_store_client_id, (int id), {
    var state = Module.anchorClientId;
    if (!state || (state.id == id && state.held)) {
        return;
    }
    if (state.release) {
        state.release();
    }
    state.id = id;
    state.held = false;
    state.release = null;
    try {
        sessionStorage.setItem('sohAnchorClientId', String(id));
    } catch (e) {
    }
    if (!navigator.locks) {
        return;
    }
    navigator.locks
        .request('soh-anchor-client-' + id, { ifAvailable : true },
                 function(lock) {
                     if (!lock || state.id != id) {
                         return;
                     }
                     state.held = true;
                     return new Promise(function(resolve) { state.release = resolve; });
                 })
        .catch(function() {});
});

// MARK: - Page events

// Hidden tabs (and locked phones) run no game frames, so others are told this player is away (Anchor::SetPaused).
// Coming back online retries a waiting reconnect straight away.
EM_JS(void, anchor_web_listen, (), {
    if (Module.anchorListening) {
        return;
    }
    Module.anchorListening = true;
    var update = function() { Module._anchor_web_set_hidden(document.visibilityState == 'hidden' ? 1 : 0); };
    document.addEventListener('visibilitychange', update);
    window.addEventListener('online', function() { Module._anchor_web_online(); });
    update();
});

extern "C" {

EMSCRIPTEN_KEEPALIVE void anchor_web_set_hidden(int hidden) {
    if (Anchor::Instance != nullptr) {
        Anchor::Instance->SetPaused(hidden != 0);
    }
}

EMSCRIPTEN_KEEPALIVE void anchor_web_online(void) {
    if (Anchor::Instance != nullptr) {
        Anchor::Instance->ReconnectNow();
    }
}

} // extern "C"

void AnchorWeb::Init() {
    anchor_web_claim_client_id();
    anchor_web_listen();
}

uint32_t AnchorWeb::GetStoredClientId() {
    return (uint32_t)anchor_web_stored_client_id();
}

void AnchorWeb::StoreClientId(uint32_t clientId) {
    anchor_web_store_client_id((int)clientId);
}

#endif
