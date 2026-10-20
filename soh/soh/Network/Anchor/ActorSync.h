#pragma once

#ifdef __cplusplus

#include <nlohmann/json.hpp>

extern "C" {
#include "z64.h"
}

/**
 * Keeps enemies and push blocks the same for everyone in a room.
 *
 * Actors are matched across clients by their room and their index in the room's spawn list, which every client
 * loads identically. Actors spawned at runtime (projectiles, children of other actors, ...) aren't synced, except
 * a few made by a spawner in the list (push blocks, Big Octo, Anubis), and the parts of a state-synced boss.
 *
 * Enemies: the player with the lowest client ID in a room runs the AI for that room's enemies and sends their
 * position, rotation and health every frame. Everyone else still runs the enemies locally, so hits and attacks keep
 * working, but follows those positions. Damage and kills from anyone are sent to everyone.
 *
 * Bosses and mini-bosses: the room owner sends the actor's state machine instead of just its position, and defeats
 * start each player's own death sequence; see the State-synced actors section in ActorSync.cpp.
 *
 * Push blocks: whoever pushes a block sends the push, and the other clients replay it. Moved blocks are also sent
 * now and then, so a player who walks in later sees them where they are.
 */
namespace AnchorActorSync {

// Game hooks, registered by Anchor while connected
void OnShouldActorUpdate(Actor* actor, bool* should);
void OnActorUpdate(Actor* actor);
void OnEnemyDefeat(Actor* actor);
void OnActorInit(Actor* actor);
void OnActorDestroy(Actor* actor);
// Once per game frame, after the local player has updated
void OnFrame();
void Reset();

// Incoming packets
void HandlePacket_ActorState(nlohmann::json& payload);
void HandlePacket_ActorHealth(nlohmann::json& payload);
void HandlePacket_ActorKill(nlohmann::json& payload);
void HandlePacket_BlockPush(nlohmann::json& payload);
void HandlePacket_BlockPos(nlohmann::json& payload);
void HandlePacket_BossState(nlohmann::json& payload);
void HandlePacket_BossHit(nlohmann::json& payload);
void HandlePacket_BossDefeated(nlohmann::json& payload);

// The ID an actor is synced by (actor ID, room and spawn list index), if it has one
bool GetActorKey(Actor* actor, uint64_t* key);

} // namespace AnchorActorSync

#endif
