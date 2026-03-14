#ifndef NETWORK_ANCHOR_ACTOR_SYNC_H
#define NETWORK_ANCHOR_ACTOR_SYNC_H
#ifdef __cplusplus

#include <cstdint>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>

extern "C" {
#include "z64.h"
}

/**
 * Unique key identifying a specific actor instance across clients.
 * Uses spawn data (actorId + params + initial position) since room
 * data is identical across all clients.
 */
struct ActorSyncID {
    s16 actorId;
    s16 params;
    Vec3s homePos;

    bool operator==(const ActorSyncID& o) const {
        return actorId == o.actorId && params == o.params &&
               homePos.x == o.homePos.x && homePos.y == o.homePos.y && homePos.z == o.homePos.z;
    }
};

struct ActorSyncIDHash {
    size_t operator()(const ActorSyncID& k) const {
        size_t h = std::hash<s16>()(k.actorId);
        h ^= std::hash<s16>()(k.params) << 1;
        h ^= std::hash<s16>()(k.homePos.x) << 2;
        h ^= std::hash<s16>()(k.homePos.y) << 3;
        h ^= std::hash<s16>()(k.homePos.z) << 4;
        return h;
    }
};

/**
 * Tracks the synced state of a single actor.
 * Stored per-actor on the authoritative client.
 */
struct ActorSyncState {
    // Ownership
    uint32_t ownerClientId = 0;       // Client that controls this actor
    float ownerDistance = 99999.0f;    // Distance from owner's Link to this actor

    // Transform
    Vec3f pos = { 0, 0, 0 };
    Vec3s rot = { 0, 0, 0 };
    Vec3f velocity = { 0, 0, 0 };
    float speedXZ = 0;

    // State
    s16 health = 0;
    bool isAlive = true;
    u32 flags = 0;

    // Shape rotation (visual, may differ from world.rot)
    Vec3s shapeRot = { 0, 0, 0 };

    // Animation (index into the actor's animation list)
    // -1 means "not tracked" — only set for actors with SkelAnime
    s16 animIndex = -1;
    float animCurFrame = 0;
    float animPlaySpeed = 0;

    // Timestamp of last update (for interpolation on receiving end)
    double lastUpdateTime = 0;
    double lastRecvTime = 0;

    // Previous position for interpolation on non-owner clients
    Vec3f prevPos = { 0, 0, 0 };
    Vec3s prevRot = { 0, 0, 0 };
};

/**
 * Categories of actors that should be synced.
 * Each category can have different sync frequencies and data.
 */
enum class ActorSyncCategory {
    None = 0,     // Not synced
    Enemy,        // ACTORCAT_ENEMY, ACTORCAT_BOSS — full state + AI authority
    NPC,          // ACTORCAT_NPC — position, rotation, head tracking
    Prop,         // ACTORCAT_PROP — pushable blocks, breakable objects
    Projectile,   // ACTORCAT_EXPLOSIVE, ACTORCAT_ITEMACTION — short-lived
    Switch,       // ACTORCAT_SWITCH — state toggles (crystal switches, etc.)
};

/**
 * Determines which sync category an actor belongs to.
 * Returns None for actors that shouldn't be synced.
 */
ActorSyncCategory GetActorSyncCategory(Actor* actor);

/**
 * Determines if an actor should be synced based on its category and state.
 */
bool ShouldSyncActor(Actor* actor);

/**
 * Determines the authority hysteresis margin for ownership transfer.
 * Returns the distance margin required before authority switches.
 */
float GetAuthorityHysteresis(ActorSyncCategory category);

/**
 * Core actor sync manager. One instance per Anchor connection.
 *
 * Responsibilities:
 * - Track which actors are synced and who owns them
 * - Compute nearest Link for each syncable actor (multi-Link targeting)
 * - Send ACTOR_SYNC packets for owned actors
 * - Receive and apply state from remote-owned actors
 * - Handle authority transfers with hysteresis
 */
class ActorSyncManager {
public:
    // Called every game frame from Anchor hooks
    void OnGameFrameUpdate(PlayState* play);

    // Called when entering a new scene/room
    void OnSceneLoad(PlayState* play);

    // Called to process an incoming ACTOR_SYNC packet
    void HandlePacket(const nlohmann::json& payload, uint32_t senderClientId);

    // Called to process an incoming ACTOR_DEATH packet (broadcast)
    void HandleDeathPacket(const nlohmann::json& payload);

    // Build outgoing packet for all locally-owned actors
    nlohmann::json BuildSyncPacket(PlayState* play);

    // Check if a specific actor is remotely owned (should skip local AI update)
    bool IsRemotelyOwned(Actor* actor);

    // Apply interpolated state to remotely-owned actors (called before draw)
    void InterpolateRemoteActors(PlayState* play, float interpFraction);

    // Compute nearest Link (local player + all DummyPlayers) for an actor
    // Updates actor->xzDistToPlayer, actor->yawTowardsPlayer, etc.
    static void ComputeNearestLink(Actor* actor, PlayState* play);

    // Clear all tracked state (on scene change, disconnect, etc.)
    void Clear();

private:
    // Map from sync ID to tracked state
    std::unordered_map<ActorSyncID, ActorSyncState, ActorSyncIDHash> mTracked;

    // Build sync ID from an actor
    static ActorSyncID MakeSyncID(Actor* actor);

    // Find the local actor matching a sync ID
    Actor* FindLocalActor(PlayState* play, const ActorSyncID& syncId);

    // Check and possibly transfer authority for an actor
    void UpdateAuthority(Actor* actor, const ActorSyncID& syncId, PlayState* play,
                         uint32_t localClientId);
};

#endif // __cplusplus
#endif // NETWORK_ANCHOR_ACTOR_SYNC_H
