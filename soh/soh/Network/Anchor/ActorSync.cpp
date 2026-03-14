#include "ActorSync.h"
#include "Anchor.h"
#include "JsonConversions.hpp"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
#include <cmath>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

// Forward declaration for DummyPlayer identification
void DummyPlayer_Update(Actor* actor, PlayState* play);

// ============================================================================
// Actor Classification
// ============================================================================

ActorSyncCategory GetActorSyncCategory(Actor* actor) {
    switch (actor->category) {
        case ACTORCAT_ENEMY:
        case ACTORCAT_BOSS:
            return ActorSyncCategory::Enemy;

        case ACTORCAT_NPC:
            // Don't sync DummyPlayers (they have their own sync)
            if (actor->update == DummyPlayer_Update) {
                return ActorSyncCategory::None;
            }
            return ActorSyncCategory::NPC;

        case ACTORCAT_PROP:
            return ActorSyncCategory::Prop;

        case ACTORCAT_BG:
            // BG actors include pushable blocks, moving platforms, etc.
            // Only sync dynamic ones (those with non-null update)
            if (actor->update != NULL) {
                return ActorSyncCategory::Prop;
            }
            return ActorSyncCategory::None;

        case ACTORCAT_SWITCH:
            return ActorSyncCategory::Switch;

        case ACTORCAT_EXPLOSIVE:
        case ACTORCAT_ITEMACTION:
            return ActorSyncCategory::Projectile;

        default:
            return ActorSyncCategory::None;
    }
}

bool ShouldSyncActor(Actor* actor) {
    if (actor == NULL || actor->update == NULL) {
        return false;
    }

    ActorSyncCategory cat = GetActorSyncCategory(actor);
    if (cat == ActorSyncCategory::None) {
        return false;
    }

    // Don't sync actors in rooms we're not in
    if (actor->room != -1 && gPlayState != NULL &&
        actor->room != gPlayState->roomCtx.curRoom.num) {
        return false;
    }

    return true;
}

float GetAuthorityHysteresis(ActorSyncCategory category) {
    switch (category) {
        case ActorSyncCategory::Enemy:
            return 50.0f;  // Enemies: 50 unit margin before authority transfer
        case ActorSyncCategory::NPC:
            return 100.0f; // NPCs: wider margin (less important to transfer quickly)
        case ActorSyncCategory::Prop:
            return 30.0f;  // Props: tighter (pushable blocks need responsive authority)
        case ActorSyncCategory::Switch:
            return 80.0f;
        case ActorSyncCategory::Projectile:
            return 20.0f;  // Projectiles: very tight (short-lived)
        default:
            return 50.0f;
    }
}

// ============================================================================
// Multi-Link Targeting
// ============================================================================

void ActorSyncManager::ComputeNearestLink(Actor* actor, PlayState* play) {
    Player* localPlayer = GET_PLAYER(play);
    f32 minDistSq = SQ(Actor_WorldDistXZToActor(actor, &localPlayer->actor)) +
                    SQ(Actor_HeightDiff(actor, &localPlayer->actor));
    Actor* nearestLink = &localPlayer->actor;

    // Check all DummyPlayers (remote Links)
    Actor* npc = play->actorCtx.actorLists[ACTORCAT_NPC].head;
    while (npc != NULL) {
        if (npc->update == DummyPlayer_Update) {
            f32 distSq = SQ(Actor_WorldDistXZToActor(actor, npc)) +
                         SQ(Actor_HeightDiff(actor, npc));
            if (distSq < minDistSq) {
                minDistSq = distSq;
                nearestLink = npc;
            }
        }
        npc = npc->next;
    }

    // Update actor fields that all enemy AI reads
    actor->xzDistToPlayer = Actor_WorldDistXZToActor(actor, nearestLink);
    actor->yDistToPlayer = Actor_HeightDiff(actor, nearestLink);
    actor->xyzDistToPlayerSq = SQ(actor->xzDistToPlayer) + SQ(actor->yDistToPlayer);
    actor->yawTowardsPlayer = Actor_WorldYawTowardActor(actor, nearestLink);
}

// ============================================================================
// Sync ID Construction & Actor Lookup
// ============================================================================

ActorSyncID ActorSyncManager::MakeSyncID(Actor* actor) {
    ActorSyncID id;
    id.actorId = actor->id;
    id.params = actor->params;
    id.homePos.x = (s16)actor->home.pos.x;
    id.homePos.y = (s16)actor->home.pos.y;
    id.homePos.z = (s16)actor->home.pos.z;
    return id;
}

Actor* ActorSyncManager::FindLocalActor(PlayState* play, const ActorSyncID& syncId) {
    // Search through all actor categories
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        Actor* actor = play->actorCtx.actorLists[cat].head;
        while (actor != NULL) {
            if (actor->id == syncId.actorId &&
                actor->params == syncId.params &&
                (s16)actor->home.pos.x == syncId.homePos.x &&
                (s16)actor->home.pos.y == syncId.homePos.y &&
                (s16)actor->home.pos.z == syncId.homePos.z) {
                return actor;
            }
            actor = actor->next;
        }
    }
    return NULL;
}

// ============================================================================
// Authority Management
// ============================================================================

void ActorSyncManager::UpdateAuthority(Actor* actor, const ActorSyncID& syncId, PlayState* play,
                                        uint32_t localClientId) {
    Player* localPlayer = GET_PLAYER(play);
    f32 localDist = Actor_WorldDistXZToActor(actor, &localPlayer->actor);

    auto it = mTracked.find(syncId);
    if (it == mTracked.end()) {
        // First time seeing this actor — claim ownership
        ActorSyncState state;
        state.ownerClientId = localClientId;
        state.ownerDistance = localDist;
        state.isAlive = (actor->update != NULL);
        mTracked[syncId] = state;
        return;
    }

    ActorSyncState& state = it->second;
    float hysteresis = GetAuthorityHysteresis(GetActorSyncCategory(actor));

    if (state.ownerClientId == localClientId) {
        // We already own it — just update distance
        state.ownerDistance = localDist;
    } else {
        // Someone else owns it — check if we should take over
        // Only take over if we're closer by the hysteresis margin
        if (localDist + hysteresis < state.ownerDistance) {
            SPDLOG_INFO("[ActorSync] Taking authority of actor {} from client {} (dist {} vs {})",
                        syncId.actorId, state.ownerClientId, localDist, state.ownerDistance);
            state.ownerClientId = localClientId;
            state.ownerDistance = localDist;
        }
    }
}

// ============================================================================
// Per-Frame Update
// ============================================================================

void ActorSyncManager::OnGameFrameUpdate(PlayState* play) {
    if (!Anchor::Instance || !Anchor::Instance->isConnected) {
        return;
    }

    uint32_t localClientId = Anchor::Instance->ownClientId;

    // Iterate all syncable actors and update authority + multi-Link targeting
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        Actor* actor = play->actorCtx.actorLists[cat].head;
        while (actor != NULL) {
            if (ShouldSyncActor(actor)) {
                ActorSyncID syncId = MakeSyncID(actor);

                // Update multi-Link targeting (nearest Link for AI)
                ActorSyncCategory syncCat = GetActorSyncCategory(actor);
                if (syncCat == ActorSyncCategory::Enemy || syncCat == ActorSyncCategory::NPC) {
                    ComputeNearestLink(actor, play);
                }

                // Update authority
                UpdateAuthority(actor, syncId, play, localClientId);
            }
            actor = actor->next;
        }
    }
}

// ============================================================================
// Packet Building (Outgoing)
// ============================================================================

nlohmann::json ActorSyncManager::BuildSyncPacket(PlayState* play) {
    nlohmann::json payload;
    payload["type"] = "ACTOR_SYNC";
    payload["sceneNum"] = play->sceneNum;
    payload["quiet"] = true;

    nlohmann::json actors = nlohmann::json::array();
    uint32_t localClientId = Anchor::Instance->ownClientId;

    for (auto& [syncId, state] : mTracked) {
        if (state.ownerClientId != localClientId) {
            continue; // Only send state for actors we own
        }

        Actor* actor = FindLocalActor(play, syncId);
        if (actor == NULL || actor->update == NULL) {
            continue;
        }

        nlohmann::json a;
        a["id"] = syncId.actorId;
        a["p"] = syncId.params;
        a["hp"] = { syncId.homePos.x, syncId.homePos.y, syncId.homePos.z };

        // Transform (compact arrays for bandwidth)
        a["pos"] = { actor->world.pos.x, actor->world.pos.y, actor->world.pos.z };
        a["rot"] = { actor->shape.rot.x, actor->shape.rot.y, actor->shape.rot.z };
        a["vel"] = { actor->velocity.x, actor->velocity.y, actor->velocity.z };
        a["spd"] = actor->speedXZ;

        // State
        a["hp2"] = actor->colChkInfo.health;
        a["fl"] = actor->flags;

        // Update tracked state
        state.pos = actor->world.pos;
        state.rot = actor->shape.rot;
        state.velocity = actor->velocity;
        state.speedXZ = actor->speedXZ;
        state.health = actor->colChkInfo.health;
        state.isAlive = (actor->update != NULL);
        state.flags = actor->flags;
        state.shapeRot = actor->shape.rot;

        actors.push_back(a);
    }

    payload["actors"] = actors;
    return payload;
}

// ============================================================================
// Packet Handling (Incoming)
// ============================================================================

void ActorSyncManager::HandlePacket(const nlohmann::json& payload, uint32_t senderClientId) {
    if (!payload.contains("actors") || !payload["actors"].is_array()) {
        return;
    }

    double now = 0;
#ifdef __EMSCRIPTEN__
    now = emscripten_get_now() / 1000.0;
#endif

    for (const auto& a : payload["actors"]) {
        ActorSyncID syncId;
        syncId.actorId = a["id"].get<s16>();
        syncId.params = a["p"].get<s16>();
        auto hp = a["hp"];
        syncId.homePos.x = hp[0].get<s16>();
        syncId.homePos.y = hp[1].get<s16>();
        syncId.homePos.z = hp[2].get<s16>();

        ActorSyncState& state = mTracked[syncId];

        // Store previous for interpolation
        state.prevPos = state.pos;
        state.prevRot = state.rot;

        // Update from packet
        auto pos = a["pos"];
        state.pos = { pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>() };
        auto rot = a["rot"];
        state.rot = { rot[0].get<s16>(), rot[1].get<s16>(), rot[2].get<s16>() };
        auto vel = a["vel"];
        state.velocity = { vel[0].get<float>(), vel[1].get<float>(), vel[2].get<float>() };
        state.speedXZ = a["spd"].get<float>();

        state.health = a["hp2"].get<s16>();
        state.flags = a["fl"].get<u32>();
        state.isAlive = (state.health > 0);

        state.ownerClientId = senderClientId;
        state.ownerDistance = 0; // Remote — we don't know their distance
        state.lastRecvTime = now;
        state.lastUpdateTime = now;

        // Apply to local actor immediately
        if (gPlayState != NULL) {
            Actor* actor = FindLocalActor(gPlayState, syncId);
            if (actor != NULL) {
                // If the actor is dead remotely but alive locally, kill it
                if (!state.isAlive && actor->update != NULL) {
                    Actor_Kill(actor);
                    continue;
                }

                // Apply remote state
                Math_Vec3f_Copy(&actor->world.pos, &state.pos);
                actor->shape.rot = state.rot;
                Math_Vec3f_Copy(&actor->velocity, &state.velocity);
                actor->speedXZ = state.speedXZ;
                actor->colChkInfo.health = state.health;
            }
        }
    }
}

void ActorSyncManager::HandleDeathPacket(const nlohmann::json& payload) {
    ActorSyncID syncId;
    syncId.actorId = payload["id"].get<s16>();
    syncId.params = payload["p"].get<s16>();
    auto hp = payload["hp"];
    syncId.homePos.x = hp[0].get<s16>();
    syncId.homePos.y = hp[1].get<s16>();
    syncId.homePos.z = hp[2].get<s16>();

    // Mark as dead in our tracking
    auto it = mTracked.find(syncId);
    if (it != mTracked.end()) {
        it->second.isAlive = false;
        it->second.health = 0;
    }

    // Kill the local actor
    if (gPlayState != NULL) {
        Actor* actor = FindLocalActor(gPlayState, syncId);
        if (actor != NULL && actor->update != NULL) {
            actor->colChkInfo.health = 0;
            // Set health to 0 — the actor's own update will handle death animation
        }
    }
}

// ============================================================================
// Remote Ownership Check
// ============================================================================

bool ActorSyncManager::IsRemotelyOwned(Actor* actor) {
    if (!Anchor::Instance || !Anchor::Instance->isConnected) {
        return false;
    }

    ActorSyncID syncId = MakeSyncID(actor);
    auto it = mTracked.find(syncId);
    if (it == mTracked.end()) {
        return false;
    }

    return it->second.ownerClientId != Anchor::Instance->ownClientId;
}

// ============================================================================
// Interpolation for Remote Actors
// ============================================================================

void ActorSyncManager::InterpolateRemoteActors(PlayState* play, float interpFraction) {
    // For remotely-owned actors, smoothly interpolate between received states
    for (auto& [syncId, state] : mTracked) {
        if (state.ownerClientId == Anchor::Instance->ownClientId) {
            continue; // We own this — no interpolation needed
        }

        Actor* actor = FindLocalActor(play, syncId);
        if (actor == NULL || actor->update == NULL) {
            continue;
        }

        // Lerp position
        float t = interpFraction;
        actor->world.pos.x = state.prevPos.x + (state.pos.x - state.prevPos.x) * t;
        actor->world.pos.y = state.prevPos.y + (state.pos.y - state.prevPos.y) * t;
        actor->world.pos.z = state.prevPos.z + (state.pos.z - state.prevPos.z) * t;

        // Shortest-path angle interpolation for rotation
        for (int i = 0; i < 3; i++) {
            s16 prev = ((s16*)&state.prevRot)[i];
            s16 curr = ((s16*)&state.rot)[i];
            s16 diff = curr - prev;
            ((s16*)&actor->shape.rot)[i] = prev + (s16)(diff * t);
        }
    }
}

// ============================================================================
// Scene Management
// ============================================================================

void ActorSyncManager::OnSceneLoad(PlayState* play) {
    Clear();
}

void ActorSyncManager::Clear() {
    mTracked.clear();
}
