#ifdef __EMSCRIPTEN__

#include "Anchor.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <emscripten.h>

extern "C" {
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
}

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

// MARK: - Automated browser tests (tools/web-test)

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* web_debug_anchor(void) {
    static std::string out;
    Anchor* anchor = Anchor::Instance;
    nlohmann::json state;
    state["enabled"] = anchor->isEnabled;
    state["connected"] = anchor->isConnected;
    state["sessionReady"] = anchor->sessionReady;
    state["clientId"] = anchor->ownClientId;
    state["ownerClientId"] = anchor->roomState.ownerClientId;
    state["paused"] = anchor->isPaused;
    state["pingMs"] = anchor->pingMs;
    state["reconnectAttempts"] = anchor->GetReconnectAttempts();
    state["status"] = anchor->GetConnectionStatus();
    state["questId"] = anchor->IsSaveLoaded() ? (int)gSaveContext.ship.quest.id : -1;
    state["questMismatches"] = anchor->questMismatches;
    state["clients"] = nlohmann::json::array();
    for (auto& [clientId, client] : anchor->clients) {
        state["clients"].push_back({ { "clientId", clientId },
                                     { "name", client.name },
                                     { "self", client.self },
                                     { "online", client.online },
                                     { "paused", client.paused },
                                     { "isSaveLoaded", client.isSaveLoaded },
                                     { "questId", client.questId },
                                     { "sceneNum", client.sceneNum },
                                     { "roomNum", client.curRoomNum },
                                     { "sceneLayer", client.sceneLayer },
                                     { "roomFrames", client.roomFrames },
                                     { "actorSync", client.actorSync },
                                     { "sameVersion", client.clientVersion == Anchor::clientVersion } });
    }
    out = state.dump();
    return out.c_str();
}

// Gives a vanilla item (GI_*) the way picking it up does, so Anchor sends it to the team
EMSCRIPTEN_KEEPALIVE int web_debug_give_gi(int getItemId) {
    if (gPlayState == nullptr) {
        return 0;
    }
    GetItemEntry entry = ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, getItemId);
    Item_Give(gPlayState, entry.itemId);
    GameInteractor_ExecuteOnItemReceiveHooks(entry);
    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_debug_has_item(int itemId) {
    return INV_CONTENT(itemId) == itemId;
}

EMSCRIPTEN_KEEPALIVE int web_debug_get_event_flag(int flag) {
    return Flags_GetEventChkInf(flag) ? 1 : 0;
}

} // extern "C"

#endif
