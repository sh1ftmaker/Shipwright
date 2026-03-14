#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>

extern "C" {
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

/**
 * ACTOR_SYNC
 *
 * Batch update for all locally-owned synced actors (enemies, NPCs, props, etc.)
 * Only sent to clients in the SAME SCENE for bandwidth efficiency.
 * Sent every game frame (20Hz during gameplay).
 */
void Anchor::SendPacket_ActorSync() {
    if (!IsSaveLoaded() || gPlayState == NULL) {
        return;
    }

    // Count clients in same scene
    uint32_t sameSceneCount = 0;
    for (auto& [clientId, client] : clients) {
        if (client.sceneNum == gPlayState->sceneNum && client.online && client.isSaveLoaded && !client.self) {
            sameSceneCount++;
        }
    }
    if (sameSceneCount == 0) {
        return;
    }

    nlohmann::json payload = actorSync.BuildSyncPacket(gPlayState);

    // Only send if there are actors to sync
    if (!payload.contains("actors") || payload["actors"].empty()) {
        return;
    }

    // Send to all clients in same scene
    for (auto& [clientId, client] : clients) {
        if (client.sceneNum == gPlayState->sceneNum && client.online && client.isSaveLoaded && !client.self) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
    }
}

/**
 * ACTOR_DEATH
 *
 * Broadcast to ALL connected clients (not just same scene) when a locally-owned
 * actor dies. This ensures death state is reliably synced even if a client
 * briefly leaves and re-enters the scene.
 */
void Anchor::SendPacket_ActorDeath(Actor* actor) {
    if (!IsSaveLoaded() || gPlayState == NULL) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = ACTOR_DEATH;
    payload["sceneNum"] = gPlayState->sceneNum;
    payload["id"] = actor->id;
    payload["p"] = actor->params;
    payload["hp"] = { (s16)actor->home.pos.x, (s16)actor->home.pos.y, (s16)actor->home.pos.z };
    payload["killerClientId"] = ownClientId;

    // Broadcast to all connected clients
    for (auto& [clientId, client] : clients) {
        if (client.online && client.isSaveLoaded && !client.self) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
    }
}

void Anchor::HandlePacket_ActorSync(nlohmann::json payload) {
    if (gPlayState == NULL) {
        return;
    }

    // Only process if we're in the same scene
    s16 senderScene = payload.value("sceneNum", (s16)SCENE_ID_MAX);
    if (senderScene != gPlayState->sceneNum) {
        return;
    }

    uint32_t senderClientId = payload.value("clientId", (uint32_t)0);
    actorSync.HandlePacket(payload, senderClientId);
}

void Anchor::HandlePacket_ActorDeath(nlohmann::json payload) {
    // Deaths are broadcast — only apply if we're in that scene
    if (gPlayState == NULL) {
        return;
    }

    s16 deathScene = payload.value("sceneNum", (s16)SCENE_ID_MAX);
    if (deathScene != gPlayState->sceneNum) {
        return;
    }

    actorSync.HandleDeathPacket(payload);
}
