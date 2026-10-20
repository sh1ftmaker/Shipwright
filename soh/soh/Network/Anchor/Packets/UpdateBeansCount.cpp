#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"

extern "C" {
#include "macros.h"
}

/**
 * UPDATE_BEANS_COUNT
 *
 * Keeps the client's bean count in sync as they plant them (buying goes through GIVE_ITEM)
 */

void Anchor::SendPacket_UpdateBeansCount() {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = UPDATE_BEANS_COUNT;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["amount"] = AMMO(ITEM_BEAN);
    payload["amountBought"] = BEANS_BOUGHT;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateBeansCount(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    // Planted by a teammate; beans are one pool for the team (see WorldSync.h)
    if (INV_CONTENT(ITEM_BEAN) != ITEM_BEAN) {
        INV_CONTENT(ITEM_BEAN) = ITEM_BEAN;
    }
    AMMO(ITEM_BEAN) = payload.at("amount").get<s8>();
    BEANS_BOUGHT = MAX(BEANS_BOUGHT, payload.at("amountBought").get<s8>());
    AnchorWorldSync::RefreshSnapshot();
}
