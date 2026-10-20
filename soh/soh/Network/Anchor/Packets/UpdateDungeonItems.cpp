#include "soh/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

extern "C" {
#include "include/macros.h"
}

/**
 * UPDATE_DUNGEON_ITEMS
 *
 * Vanilla dungeon items (map, compass, boss key) and small key changes.
 *
 * Map, compass and boss key bits are OR'd in, so two pickups at once don't lose one. Small keys travel as changes,
 * +1 for a pickup and -1 for a door, never as a count: counts sent by two players at once overwrite each other and
 * never converge. Each change names its source flag (the chest or collectible the key came from, the door it
 * opened); a teammate who set that flag themselves made the same change already and skips it. Rando keys arrive
 * through GIVE_ITEM the same way, and are spent through here.
 *
 * Counts are only compared in the team state merge (UpdateTeamState.cpp), when someone joins.
 */

void Anchor::SendPacket_UpdateDungeonItems(s8 keyDelta, AnchorWorldSync::FlagRef source) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = UPDATE_DUNGEON_ITEMS;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["mapIndex"] = gSaveContext.mapIndex;
    payload["dungeonItems"] = gSaveContext.inventory.dungeonItems[gSaveContext.mapIndex];
    payload["keyDelta"] = keyDelta;
    AnchorWorldSync::WriteSource(payload, source);

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateDungeonItems(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    u16 mapIndex = payload.at("mapIndex").get<u16>();
    // dungeonKeys is shorter than dungeonItems (19 vs 20), so bound by the smaller of the two.
    if (mapIndex >= ARRAY_COUNT(gSaveContext.inventory.dungeonItems) ||
        mapIndex >= ARRAY_COUNT(gSaveContext.inventory.dungeonKeys)) {
        SPDLOG_ERROR("[Anchor] UPDATE_DUNGEON_ITEMS: mapIndex {} out of range", mapIndex);
        return;
    }
    gSaveContext.inventory.dungeonItems[mapIndex] |= payload.at("dungeonItems").get<u8>();

    s8 keyDelta = payload.value("keyDelta", (s8)0);
    if (keyDelta == 0) {
        return;
    }
    AnchorWorldSync::FlagRef source = AnchorWorldSync::ReadSource(payload);
    if (AnchorWorldSync::ClaimedLocally(source)) {
        return;
    }
    AnchorWorldSync::ApplyKeyDelta(mapIndex, keyDelta);
    AnchorWorldSync::SetFlagFromTeammate(source);
}
