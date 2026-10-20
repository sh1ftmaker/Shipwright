#include "ActorSync.h"
#include "Anchor.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <cmath>
#include <climits>
#include <algorithm>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"
#include "src/overlays/actors/ovl_Boss_Goma/z_boss_goma.h"
#include "soh/ResourceManagerHelpers.h"
void SkelAnime_SetUpdate(SkelAnime* skelAnime);
extern PlayState* gPlayState;
void ObjOshihiki_OnScene(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_OnActor(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_Push(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_SetupPush(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_SetupFall(ObjOshihiki* block, PlayState* play);
void BossGoma_Defeated(BossGoma* goma, PlayState* play);
void BossGoma_SetupDefeated(BossGoma* goma, PlayState* play);
void BossGoma_FloorAttackPosture(BossGoma* goma, PlayState* play);
void BossGoma_FloorPrepareAttack(BossGoma* goma, PlayState* play);
void BossGoma_FloorAttack(BossGoma* goma, PlayState* play);
void BossGoma_FloorDamaged(BossGoma* goma, PlayState* play);
void BossGoma_FloorLandStruckDown(BossGoma* goma, PlayState* play);
void BossGoma_FloorLand(BossGoma* goma, PlayState* play);
void BossGoma_FloorStunned(BossGoma* goma, PlayState* play);
void BossGoma_FallJump(BossGoma* goma, PlayState* play);
void BossGoma_FallStruckDown(BossGoma* goma, PlayState* play);
void BossGoma_CeilingSpawnGohmas(BossGoma* goma, PlayState* play);
void BossGoma_CeilingPrepareSpawnGohmas(BossGoma* goma, PlayState* play);
void BossGoma_FloorIdle(BossGoma* goma, PlayState* play);
void BossGoma_CeilingIdle(BossGoma* goma, PlayState* play);
void BossGoma_FloorMain(BossGoma* goma, PlayState* play);
void BossGoma_WallClimb(BossGoma* goma, PlayState* play);
void BossGoma_CeilingMoveToCenter(BossGoma* goma, PlayState* play);
void CollisionCheck_ApplyDamage(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider,
                                ColliderInfo* info);
}

namespace AnchorActorSync {

static const std::string ACTOR_STATE = "ACTOR_STATE";
static const std::string ACTOR_HEALTH = "ACTOR_HEALTH";
static const std::string ACTOR_KILL = "ACTOR_KILL";
static const std::string BLOCK_PUSH = "BLOCK_PUSH";
static const std::string BLOCK_POS = "BLOCK_POS";
static const std::string BOSS_STATE = "BOSS_STATE";
static const std::string BOSS_HIT = "BOSS_HIT";
static const std::string BOSS_DEFEATED = "BOSS_DEFEATED";

namespace {

// All in game frames, 20 per second
constexpr uint32_t STATE_TIMEOUT = 40;        // A room owner's enemy state stays in charge this long
constexpr uint32_t STATE_REFRESH = 10;        // Resend enemies that haven't changed this often
constexpr uint32_t HEALTH_RESEND = 20;        // Remind the room owner about damage it hasn't applied
constexpr uint32_t SLOW_TICK = 40;            // Moved blocks and dead enemies are resent this often
constexpr uint32_t BLOCK_SETTLE_FRAMES = 60;  // Ignore block positions this long after a push
constexpr uint32_t ZERO_HEALTH_GRACE = 30;    // Someone else's enemy at 0 health gets this long to die by itself

// Actor ID, room and spawn list index. The ID makes sure two clients never match different actors (a different scene
// layer, or an actor that only one of them spawned). Sent as a JSON number, so it must stay below 2^53.
typedef uint64_t Key;

struct RemoteEnemy {
    Vec3f pos;
    s16 shapeRotY;
    s16 worldRotY;
    s16 health;
    uint32_t frame;
};

struct SentEnemy {
    Vec3s pos;
    s16 shapeRotY;
    s16 worldRotY;
    s16 health;
    uint32_t frame;
};

struct Block {
    Vec3f spawnPos;
    ObjOshihikiActionFunc prevFunc = nullptr;
    bool remotePush = false; // The push in progress came from another player, so don't send it back
    uint32_t lastPushFrame = 0;
    std::deque<nlohmann::json> pendingPushes; // Pushes that arrived while the block was busy
};

uint32_t sFrame = 0;
s16 sScene = -1;
uint32_t sPeersFrame = UINT32_MAX;
bool sHasPeers = false;
std::unordered_map<Key, RemoteEnemy> sRemote;
std::unordered_map<Key, SentEnemy> sSent;
std::unordered_map<Key, s16> sLastHealth;
std::unordered_map<Key, uint32_t> sHealthSentFrame;
std::unordered_set<Key> sDead;       // Killed by anyone this visit
std::unordered_set<Key> sKilledHere; // ...of those, the ones this game killed, which play out their own death
// Enemies that someone else took to 0 health, and when. Some deaths aren't defeats (a Freezard melting), so this game
// may never hear more about them.
std::unordered_map<Key, uint32_t> sRemoteZero;
std::unordered_map<Key, Block> sBlocks;
// IDs for actors that aren't in a spawn list but are made by one that is, worked out when they spawn
std::unordered_map<Actor*, Key> sDerivedKeys;

bool HasStateSyncDef(Actor* actor);

Key MakeKey(s16 actorId, s8 room, s16 index) {
    return ((Key)(u16)actorId << 24) | ((Key)(u8)room << 16) | (u16)index;
}

s8 KeyRoom(Key key) {
    return (s8)(key >> 16);
}

// Only actors from a room's spawn list have an ID every client agrees on
bool GetKey(Actor* actor, Key* key) {
    if (actor->room < 0) {
        return false;
    }
    s16 index = GetActorListIndex(actor);
    if (index < 0) {
        auto derived = sDerivedKeys.find(actor);
        if (derived == sDerivedKeys.end()) {
            return false;
        }
        *key = derived->second;
        return true;
    }
    *key = MakeKey(actor->id, actor->room, index);
    return true;
}

// Actors made during play by one from a spawn list (one each) take their ID from it, with the slot telling apart
// several of the same kind. Worked out on the child's first update: its init runs inside the spawn call, before the
// spawner is set as its parent.
struct DerivedKeyRule {
    s16 childId;
    s16 parentId;
    u8 (*slot)(Actor* child); // 0-63; nullptr for 0
};

const DerivedKeyRule sDerivedKeyRules[] = {
    { ACTOR_OBJ_OSHIHIKI, ACTOR_OBJ_MAKEOSHIHIKI, nullptr }, // Push blocks
    { ACTOR_EN_BIGOKUTA, ACTOR_BG_BDAN_OBJECTS, nullptr },   // Big Octo, from its platform
    { ACTOR_EN_ANUBICE, ACTOR_EN_ANUBICE_TAG, nullptr },     // Anubis, from its trigger
};

void DeriveKey(Actor* actor) {
    if (actor->room < 0 || actor->parent == nullptr || sDerivedKeys.contains(actor) ||
        GetActorListIndex(actor) >= 0) {
        return;
    }
    for (const DerivedKeyRule& rule : sDerivedKeyRules) {
        if (actor->id != rule.childId || actor->parent->id != rule.parentId) {
            continue;
        }
        s16 parentIndex = GetActorListIndex(actor->parent);
        if (parentIndex >= 0 && parentIndex < 0x100) {
            u8 slot = rule.slot != nullptr ? rule.slot(actor) & 0x3F : 0;
            sDerivedKeys[actor] = MakeKey(actor->id, actor->room, 0x4000 | slot << 8 | parentIndex);
        }
        return;
    }
}

bool IsEnabled() {
    Anchor* anchor = Anchor::Instance;
    return anchor != nullptr && anchor->isConnected && anchor->roomState.syncActors && anchor->IsSaveLoaded();
}

bool IsPeer(const AnchorClient& client) {
    return !client.self && client.online && client.isSaveLoaded && client.actorSync &&
           client.sceneNum == gPlayState->sceneNum && client.sceneLayer == gSaveContext.sceneLayer &&
           client.clientVersion == Anchor::clientVersion;
}

bool HasPeers() {
    if (sPeersFrame != sFrame) {
        sPeersFrame = sFrame;
        sHasPeers = false;
        for (auto& [clientId, client] : Anchor::Instance->clients) {
            if (IsPeer(client)) {
                sHasPeers = true;
                break;
            }
        }
    }
    return sHasPeers;
}

bool IsRoomOccupied(s8 room) {
    if (gPlayState->roomCtx.curRoom.num == room) {
        return true;
    }
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (IsPeer(client) && client.curRoomNum == room) {
            return true;
        }
    }
    return false;
}

// The player with the lowest client ID in a room runs its enemies. When nobody is in it, everyone runs their own.
uint32_t RoomOwner(s8 room) {
    Anchor* anchor = Anchor::Instance;
    uint32_t owner = UINT32_MAX;
    if (gPlayState->roomCtx.curRoom.num == room) {
        owner = anchor->ownClientId;
    }
    for (auto& [clientId, client] : anchor->clients) {
        if (IsPeer(client) && client.curRoomNum == room && clientId < owner) {
            owner = clientId;
        }
    }
    return owner == UINT32_MAX ? anchor->ownClientId : owner;
}

bool OwnsRoom(s8 room) {
    return RoomOwner(room) == Anchor::Instance->ownClientId;
}

void SendToPeers(nlohmann::json& payload) {
    payload["sceneNum"] = gPlayState->sceneNum;
    payload["quiet"] = true;
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (IsPeer(client)) {
            payload["targetClientId"] = clientId;
            Anchor::Instance->SendJsonToRemote(payload);
        }
    }
}

bool IsForThisScene(nlohmann::json& payload) {
    return IsEnabled() && payload.value("sceneNum", (s16)SCENE_ID_MAX) == gPlayState->sceneNum;
}

Actor* FindActor(Key key) {
    for (int category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != nullptr; actor = actor->next) {
            Key actorKey;
            if (actor->room == KeyRoom(key) && GetKey(actor, &actorKey) && actorKey == key) {
                return actor;
            }
        }
    }
    return nullptr;
}

void CheckScene() {
    if (gPlayState->sceneNum != sScene) {
        Reset();
        sScene = gPlayState->sceneNum;
    }
}

// MARK: - Enemies

// Things that are enemies only some of the time, or that only spawn or time others
bool IsSyncedEnemy(Actor* actor) {
    if (actor->category != ACTORCAT_ENEMY || HasStateSyncDef(actor)) {
        return false;
    }
    switch (actor->id) {
        case ACTOR_EN_ZL3:
        case ACTOR_EN_DAIKU:
        case ACTOR_EN_ELF:
        case ACTOR_EN_DNT_NOMAL:
        case ACTOR_EN_SHOPNUTS:
        case ACTOR_EN_BOM:
        case ACTOR_EN_BOMBF:
        case ACTOR_EN_ATTACK_NIW:
        case ACTOR_EN_ENCOUNT1:
        case ACTOR_EN_ENCOUNT2:
        case ACTOR_OBJ_ROOMTIMER:
            return false;
        default:
            return true;
    }
}

// Enemies go after whichever Link is closest, not just the local one
void TargetNearestLink(Actor* actor) {
    Player* player = GET_PLAYER(gPlayState);
    Actor* nearest = &player->actor;
    f32 nearestDistSq = Math3D_Vec3fDistSq(&actor->world.pos, &player->actor.world.pos);

    for (Actor* npc = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; npc != nullptr; npc = npc->next) {
        if (npc->update != DummyPlayer_Update) {
            continue;
        }
        auto client = Anchor::Instance->clients.find(Anchor::Instance->GetDummyPlayerClientId(npc));
        if (client == Anchor::Instance->clients.end() || !client->second.online || !client->second.isSaveLoaded ||
            client->second.sceneNum != gPlayState->sceneNum) {
            continue;
        }
        f32 distSq = Math3D_Vec3fDistSq(&actor->world.pos, &npc->world.pos);
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = npc;
        }
    }

    if (nearest != &player->actor) {
        actor->xzDistToPlayer = Actor_WorldDistXZToActor(actor, nearest);
        actor->yDistToPlayer = Actor_HeightDiff(actor, nearest);
        actor->xyzDistToPlayerSq = SQ(actor->xzDistToPlayer) + SQ(actor->yDistToPlayer);
        actor->yawTowardsPlayer = Actor_WorldYawTowardActor(actor, nearest);
    }
}

// Pull a locally run enemy to where the room owner has it
void FollowRoomOwner(Actor* actor, Key key) {
    auto it = sRemote.find(key);
    if (it == sRemote.end() || sFrame - it->second.frame > STATE_TIMEOUT) {
        return;
    }
    RemoteEnemy& remote = it->second;

    if (Math3D_Vec3fDistSq(&actor->world.pos, &remote.pos) > SQ(150.0f)) {
        actor->world.pos = remote.pos;
    } else {
        actor->world.pos.x += (remote.pos.x - actor->world.pos.x) * 0.5f;
        actor->world.pos.y += (remote.pos.y - actor->world.pos.y) * 0.5f;
        actor->world.pos.z += (remote.pos.z - actor->world.pos.z) * 0.5f;
    }
    actor->shape.rot.y = remote.shapeRotY;
    actor->world.rot.y = remote.worldRotY;

    if (remote.health < actor->colChkInfo.health) {
        actor->colChkInfo.health = MAX(remote.health, 0);
        sLastHealth[key] = actor->colChkInfo.health;
        if (actor->colChkInfo.health == 0) {
            sRemoteZero.emplace(key, sFrame);
        }
    }
}

void KillEnemy(Actor* actor, bool withEffect) {
    if (withEffect) {
        static Vec3f zero = { 0.0f, 0.0f, 0.0f };
        Vec3f pos = actor->world.pos;
        pos.y += 20.0f;
        EffectSsDeadDb_Spawn(gPlayState, &pos, &zero, &zero, 150, 0, 255, 255, 255, 255, 0, 0, 255, 1, 9, true);
        SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 40, NA_SE_EN_EXTINCT);
    }
    Actor_Kill(actor);
}

void SendHealth(Key key, s16 health) {
    sHealthSentFrame[key] = sFrame;
    nlohmann::json payload;
    payload["type"] = ACTOR_HEALTH;
    payload["key"] = key;
    payload["health"] = health;
    SendToPeers(payload);
}

void EnemyPreUpdate(Actor* actor, Key key, bool* should) {
    if (sDead.contains(key) && !sKilledHere.contains(key)) {
        // Someone else already killed it this visit
        Actor_Kill(actor);
        *should = false;
        return;
    }
    // Someone else took it to 0 health, so its own death never started here
    auto zero = sRemoteZero.find(key);
    if (zero != sRemoteZero.end() && actor->colChkInfo.health == 0 && sFrame - zero->second > ZERO_HEALTH_GRACE) {
        sDead.insert(key);
        KillEnemy(actor, true);
        *should = false;
        return;
    }
    TargetNearestLink(actor);
    if (!OwnsRoom(actor->room)) {
        FollowRoomOwner(actor, key);
    }
}

void EnemyPostUpdate(Actor* actor, Key key) {
    if (actor->update == nullptr) {
        return;
    }
    bool ownsRoom = OwnsRoom(actor->room);
    if (!ownsRoom) {
        FollowRoomOwner(actor, key);
    }

    // Damage dealt here goes to everyone else
    s16 health = actor->colChkInfo.health;
    auto last = sLastHealth.find(key);
    if (last == sLastHealth.end()) {
        sLastHealth[key] = health;
    } else if (health < last->second) {
        last->second = health;
        SendHealth(key, health);
    } else {
        last->second = health;
    }

    // Keep reminding the room owner of damage it hasn't applied yet (lost packet, or it just took over the room)
    if (!ownsRoom) {
        auto remote = sRemote.find(key);
        if (remote != sRemote.end() && sFrame - remote->second.frame <= STATE_TIMEOUT &&
            remote->second.health > health && sFrame - sHealthSentFrame[key] >= HEALTH_RESEND) {
            SendHealth(key, health);
        }
    }
}

void SendEnemyStates() {
    nlohmann::json list = nlohmann::json::array();
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != nullptr; actor = actor->next) {
        Key key;
        if (actor->update == nullptr || !IsSyncedEnemy(actor) || !GetKey(actor, &key) || sDead.contains(key) ||
            !OwnsRoom(actor->room)) {
            continue;
        }

        SentEnemy now = { { (s16)lroundf(actor->world.pos.x), (s16)lroundf(actor->world.pos.y),
                            (s16)lroundf(actor->world.pos.z) },
                          actor->shape.rot.y,
                          actor->world.rot.y,
                          actor->colChkInfo.health,
                          sFrame };
        auto sent = sSent.find(key);
        if (sent != sSent.end() && sFrame - sent->second.frame < STATE_REFRESH &&
            sent->second.pos.x == now.pos.x && sent->second.pos.y == now.pos.y && sent->second.pos.z == now.pos.z &&
            sent->second.shapeRotY == now.shapeRotY && sent->second.worldRotY == now.worldRotY &&
            sent->second.health == now.health) {
            continue;
        }
        sSent[key] = now;
        list.push_back({ key, now.pos.x, now.pos.y, now.pos.z, now.shapeRotY, now.worldRotY,
                         now.health });
    }

    if (!list.empty()) {
        nlohmann::json payload;
        payload["type"] = ACTOR_STATE;
        payload["actors"] = list;
        SendToPeers(payload);
    }
}

void SendDeadList() {
    if (sDead.empty()) {
        return;
    }
    nlohmann::json list = nlohmann::json::array();
    for (Key key : sDead) {
        list.push_back(key);
    }
    nlohmann::json payload;
    payload["type"] = ACTOR_KILL;
    payload["actors"] = list;
    payload["effect"] = false;
    SendToPeers(payload);
}

// Enemies respawn when their room loads again, like in single player, once nobody is left in it
void ForgetDeadInEmptyRooms() {
    std::erase_if(sDead, [](Key key) { return !IsRoomOccupied(KeyRoom(key)); });
    std::erase_if(sKilledHere, [](Key key) { return !IsRoomOccupied(KeyRoom(key)); });
    std::erase_if(sRemoteZero, [](const auto& entry) { return !IsRoomOccupied(KeyRoom(entry.first)); });
}

// MARK: - Push blocks

bool IsBlockIdle(ObjOshihiki* block) {
    return block->actionFunc == ObjOshihiki_OnScene || block->actionFunc == ObjOshihiki_OnActor;
}

Block& TrackBlock(Actor* actor, Key key) {
    auto it = sBlocks.find(key);
    if (it == sBlocks.end()) {
        it = sBlocks.emplace(key, Block()).first;
        it->second.spawnPos = actor->world.pos;
        it->second.prevFunc = ((ObjOshihiki*)actor)->actionFunc;
    }
    return it->second;
}

// Start the same push the other player made, from where they started it
void ApplyBlockPush(ObjOshihiki* block, Block& tracked, nlohmann::json& payload) {
    Actor* actor = &block->dyna.actor;
    auto from = payload["from"];
    actor->home.pos = { from[0].get<f32>(), from[1].get<f32>(), from[2].get<f32>() };
    actor->world.pos = actor->home.pos;
    f32 direction = payload["direction"].get<f32>();
    s16 angle = payload["angle"].get<s16>();
    block->dyna.unk_150 = direction;
    block->dyna.unk_158 = angle;
    actor->world.rot.y = angle;
    block->yawSin = Math_SinS(angle);
    block->yawCos = Math_CosS(angle);
    block->direction = direction;
    block->pushDist = 0.0f;
    block->pushSpeed = 0.0f;
    block->timer = 0;
    ObjOshihiki_SetupPush(block, gPlayState);
    tracked.remotePush = true;
    tracked.lastPushFrame = sFrame;
}

void BlockPreUpdate(Actor* actor, Key key) {
    ObjOshihiki* block = (ObjOshihiki*)actor;
    Block& tracked = TrackBlock(actor, key);
    if (!tracked.pendingPushes.empty() && IsBlockIdle(block)) {
        ApplyBlockPush(block, tracked, tracked.pendingPushes.front());
        tracked.pendingPushes.pop_front();
    }
}

void BlockPostUpdate(Actor* actor, Key key) {
    ObjOshihiki* block = (ObjOshihiki*)actor;
    Block& tracked = TrackBlock(actor, key);

    if (block->actionFunc == ObjOshihiki_Push && tracked.prevFunc != ObjOshihiki_Push) {
        tracked.lastPushFrame = sFrame;
        if (!tracked.remotePush) {
            // The local player started pushing it. Nothing has moved yet, so home is still the start position.
            nlohmann::json payload;
            payload["type"] = BLOCK_PUSH;
            payload["key"] = key;
            payload["from"] = { actor->home.pos.x, actor->home.pos.y, actor->home.pos.z };
            payload["direction"] = block->direction;
            payload["angle"] = block->dyna.unk_158;
            SendToPeers(payload);
        }
    }
    if (block->actionFunc != ObjOshihiki_Push) {
        tracked.remotePush = false;
    }
    tracked.prevFunc = block->actionFunc;
}

void SendBlockPositions() {
    nlohmann::json list = nlohmann::json::array();
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_PROP].head; actor != nullptr; actor = actor->next) {
        Key key;
        if (actor->id != ACTOR_OBJ_OSHIHIKI || actor->update == nullptr || !GetKey(actor, &key)) {
            continue;
        }
        auto tracked = sBlocks.find(key);
        if (tracked == sBlocks.end() || !IsBlockIdle((ObjOshihiki*)actor) ||
            Math3D_Vec3fDistSq(&actor->world.pos, &tracked->second.spawnPos) <= 1.0f) {
            continue;
        }
        list.push_back({ key, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z });
    }
    if (!list.empty()) {
        nlohmann::json payload;
        payload["type"] = BLOCK_POS;
        payload["blocks"] = list;
        SendToPeers(payload);
    }
}

// MARK: - State-synced actors

/**
 * Bosses and mini-bosses run a state machine that following a position can't keep in step, so the room owner sends
 * the actual state instead: action, timers, animation and transform. Everyone runs the fight from that state, so hits
 * land the same way for everyone; a hit that changes the state on another player's game (a stun, damage) is sent back
 * to the room owner, who carries on from it. Intros and deaths use a camera that belongs to each player's own game, so
 * those play locally: only the fight is shared, and a defeat starts everyone's own death (with its cutscene, blue warp
 * and heart container, or its drop and switch flag).
 *
 * Each state is applied once, when it arrives, between two game frames; the actor runs by itself until the next one.
 * Hits are found by comparing each actor with a copy taken before the frame's updates, so a hit that another actor
 * deals (a reflected projectile, part of the same boss) counts as well as one from the actor's own collision check.
 *
 * This relies on both players running the same build, which Anchor already requires: function and animation pointers
 * are the same in both.
 */
typedef void (*ActionFunc)(void*, PlayState*);

// Bytes copied as they are, so never a pointer (or anything holding one)
struct SyncField {
    u16 offset;
    u16 size;
    u8 mask; // Copies only these bits of a byte of flags (0: the whole field)
};

#define ACTION(func) ((ActionFunc)(func))
#define FIELD(type, member) SyncField{ offsetof(type, member), sizeof(((type*)nullptr)->member), 0 }
// From one member to the end of another, for a run of plain numbers
#define FIELDS(type, first, last) \
    SyncField{ offsetof(type, first), offsetof(type, last) + sizeof(((type*)nullptr)->last) - offsetof(type, first), 0 }
#define FLAGS(type, member, bits) SyncField{ offsetof(type, member), 1, (u8)(bits) }

constexpr size_t NO_ACTION = SIZE_MAX;

struct HealthSpec {
    u16 offset = offsetof(Actor, colChkInfo.health);
    u8 size = 1; // 1: u8, 2: s16
};

struct StateSyncDef;

struct GroupMember {
    const StateSyncDef* def;
    Actor* (*find)(Actor* leader); // nullptr while it isn't there
};

struct StateSyncDef {
    const char* name;
    s16 actorId;
    size_t size;                                  // sizeof its struct
    bool (*match)(Actor* actor) = nullptr;        // Which actors with this ID (parts and projectiles share some); all if not set
    size_t actionOffset = NO_ACTION;
    std::vector<ActionFunc> fightActions = {};    // Sent as an index into this list
    std::vector<ActionFunc> hitReactions = {};    // Actions a hit puts it in
    std::vector<ActionFunc> localActions = {};    // Fight actions each game plays out by itself (grabbing its own Link)
    std::vector<ActionFunc> deathActions = {};
    bool (*isFighting)(Actor* actor) = nullptr;   // Default: in one of the fight actions
    bool (*isDefeated)(Actor* actor) = nullptr;   // Default: in one of the death actions
    bool (*wasHit)(Actor* actor, const void* before) = nullptr; // Hits that change neither the action nor health
    std::vector<size_t> skelAnimes = {};
    HealthSpec health = {};
    bool healthFollowsOwner = false; // Health can go back up (otherwise the lowest anyone has wins)
    std::vector<SyncField> fields = {};
    void (*onApply)(Actor* actor, const void* before) = nullptr; // After taking someone else's state, e.g. to start effects
    void (*defeat)(Actor* actor) = nullptr;       // Starts the death on a game that didn't land the blow
    std::vector<GroupMember> members = {};        // Other actors in the same fight, sent along with this one
    bool isMember = false;                        // Only synced as part of another actor's group
    bool targetsOwnLink = false;                  // Skip TargetNearestLink
};

// Each actor as its def describes it, plus a copy of its struct from before the frame's updates
struct Synced {
    const StateSyncDef* def = nullptr;
    std::vector<Actor*> actors;          // The leader, then its members (nullptr when not there)
    std::vector<std::vector<u8>> before; // Each as it was at the last check
    uint32_t seq = 0;                    // Room owner: the last state it sent. Others: the last one applied
    uint32_t seqFrom = 0;                // ...and who sent that
    uint32_t hitSeq = 0;                 // The last hit this game sent
    uint32_t holdUntil = 0;              // Until then, the room owner's states are ignored unless they include that hit
    std::unordered_map<uint32_t, uint32_t> hitsTaken; // Room owner: the last hit from each player it has taken
    bool defeated = false;               // Seen or heard about, so it's sent once
    bool pendingDefeat = false;          // Defeated elsewhere while this game was still in the intro
};

constexpr uint32_t SYNC_HIT_HOLD = 40;

std::unordered_map<Key, Synced> sSynced;

const std::vector<const StateSyncDef*>& StateSyncDefs();

const StateSyncDef* FindDef(Actor* actor) {
    static std::unordered_map<s16, std::vector<const StateSyncDef*>> byId;
    if (byId.empty()) {
        for (const StateSyncDef* def : StateSyncDefs()) {
            byId[def->actorId].push_back(def);
        }
    }
    auto it = byId.find(actor->id);
    if (it == byId.end()) {
        return nullptr;
    }
    for (const StateSyncDef* def : it->second) {
        if (def->match == nullptr || def->match(actor)) {
            return def;
        }
    }
    return nullptr;
}

bool HasStateSyncDef(Actor* actor) {
    return FindDef(actor) != nullptr;
}

ActionFunc ActionOf(Actor* actor, const StateSyncDef& def) {
    return def.actionOffset == NO_ACTION ? nullptr : *(ActionFunc*)((u8*)actor + def.actionOffset);
}

bool IsIn(const std::vector<ActionFunc>& list, ActionFunc action) {
    return std::find(list.begin(), list.end(), action) != list.end();
}

int ActionIndex(Actor* actor, const StateSyncDef& def) {
    if (def.actionOffset == NO_ACTION) {
        return 0;
    }
    auto it = std::find(def.fightActions.begin(), def.fightActions.end(), ActionOf(actor, def));
    return it == def.fightActions.end() ? -1 : (int)(it - def.fightActions.begin());
}

bool IsFighting(Actor* actor, const StateSyncDef& def) {
    return def.isFighting != nullptr ? def.isFighting(actor) : ActionIndex(actor, def) >= 0;
}

bool IsDefeated(Actor* actor, const StateSyncDef& def) {
    return def.isDefeated != nullptr ? def.isDefeated(actor) : IsIn(def.deathActions, ActionOf(actor, def));
}

s32 ReadHealthAt(const u8* at, const StateSyncDef& def) {
    return def.health.size == 2 ? *(const s16*)at : *at;
}

s32 ReadHealth(const void* base, const StateSyncDef& def) {
    return ReadHealthAt((const u8*)base + def.health.offset, def);
}

void WriteHealth(void* base, const StateSyncDef& def, s32 health) {
    u8* at = (u8*)base + def.health.offset;
    if (def.health.size == 2) {
        *(s16*)at = (s16)health;
    } else {
        *at = (u8)CLAMP(health, 0, 255);
    }
}

// The animation itself is sent as its pointer, which is the same in every game
struct SkelState {
    void* animation;
    u8 mode;
    f32 startFrame, endFrame, animLength, curFrame, playSpeed;
};

struct ActorBase {
    Vec3f pos;
    Vec3s rot, shapeRot;
    Vec3f velocity;
    f32 speedXZ, gravity;
};

size_t SnapshotSize(const StateSyncDef& def) {
    size_t size = sizeof(ActorBase) + def.health.size + def.skelAnimes.size() * sizeof(SkelState);
    for (const SyncField& field : def.fields) {
        size += field.size;
    }
    return size;
}

std::vector<u8> Snapshot(Actor* actor, const StateSyncDef& def) {
    std::vector<u8> out;
    out.reserve(SnapshotSize(def));
    auto put = [&](const void* data, size_t size) { out.insert(out.end(), (const u8*)data, (const u8*)data + size); };

    ActorBase base = { actor->world.pos, actor->world.rot, actor->shape.rot, actor->velocity, actor->speedXZ,
                       actor->gravity };
    put(&base, sizeof(base));
    put((u8*)actor + def.health.offset, def.health.size);
    for (const SyncField& field : def.fields) {
        put((u8*)actor + field.offset, field.size);
    }
    for (size_t offset : def.skelAnimes) {
        SkelAnime* skel = (SkelAnime*)((u8*)actor + offset);
        SkelState state = { skel->animation, skel->mode,       skel->startFrame, skel->endFrame,
                            skel->animLength, skel->curFrame, skel->playSpeed };
        put(&state, sizeof(state));
    }
    return out;
}

void ApplySnapshot(Actor* actor, const StateSyncDef& def, int action, const std::vector<u8>& data) {
    const u8* in = data.data();
    auto get = [&](void* out, size_t size) {
        memcpy(out, in, size);
        in += size;
    };
    if (def.actionOffset != NO_ACTION) {
        *(ActionFunc*)((u8*)actor + def.actionOffset) = def.fightActions[action];
    }

    ActorBase base;
    get(&base, sizeof(base));
    actor->world.pos = base.pos;
    actor->world.rot = base.rot;
    actor->shape.rot = base.shapeRot;
    actor->velocity = base.velocity;
    actor->speedXZ = base.speedXZ;
    actor->gravity = base.gravity;

    u8 health[2];
    get(health, def.health.size);
    s32 remoteHealth = ReadHealthAt(health, def);
    if (def.healthFollowsOwner || remoteHealth < ReadHealth(actor, def)) {
        WriteHealth(actor, def, remoteHealth);
    }

    for (const SyncField& field : def.fields) {
        u8* at = (u8*)actor + field.offset;
        if (field.mask != 0) {
            *at = (*at & ~field.mask) | (*in & field.mask);
            in++;
        } else {
            get(at, field.size);
        }
    }

    for (size_t offset : def.skelAnimes) {
        SkelAnime* skel = (SkelAnime*)((u8*)actor + offset);
        SkelState state;
        get(&state, sizeof(state));
        // A different animation blends in from the current pose, like a local animation change would
        if ((state.animation != skel->animation || state.mode != skel->mode) &&
            ResourceMgr_OTRSigCheck((char*)state.animation)) {
            skel->animation = state.animation;
            skel->mode = state.mode;
            SkelAnime_SetUpdate(skel);
        }
        skel->startFrame = state.startFrame;
        skel->endFrame = state.endFrame;
        skel->animLength = state.animLength;
        skel->curFrame = state.curFrame;
        skel->playSpeed = state.playSpeed;
    }
}

std::string Base64(const std::vector<u8>& data) {
    static const char* digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        u32 n = data[i] << 16 | (i + 1 < data.size() ? data[i + 1] << 8 : 0) | (i + 2 < data.size() ? data[i + 2] : 0);
        out += digits[n >> 18 & 63];
        out += digits[n >> 12 & 63];
        out += i + 1 < data.size() ? digits[n >> 6 & 63] : '=';
        out += i + 2 < data.size() ? digits[n & 63] : '=';
    }
    return out;
}

std::vector<u8> Unbase64(const std::string& text) {
    std::vector<u8> out;
    u32 n = 0;
    int bits = 0;
    for (char c : text) {
        int digit = c >= 'A' && c <= 'Z'   ? c - 'A'
                    : c >= 'a' && c <= 'z' ? c - 'a' + 26
                    : c >= '0' && c <= '9' ? c - '0' + 52
                    : c == '+'             ? 62
                    : c == '/'             ? 63
                                           : -1;
        if (digit < 0) {
            break;
        }
        n = n << 6 | digit;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(n >> bits & 0xFF);
        }
    }
    return out;
}

const StateSyncDef& MemberDef(const Synced& synced, size_t i) {
    return i == 0 ? *synced.def : *synced.def->members[i - 1].def;
}

// The leader's actor is found by its key, the members through the leader
void FindMembers(Synced& synced, Actor* leader) {
    synced.actors.resize(synced.def->members.size() + 1);
    synced.before.resize(synced.actors.size());
    synced.actors[0] = leader;
    for (size_t i = 1; i < synced.actors.size(); i++) {
        Actor* member = synced.def->members[i - 1].find(leader);
        if (member != synced.actors[i]) {
            synced.before[i].clear();
        }
        synced.actors[i] = member;
    }
}

Actor* Leader(const Synced& synced) {
    return synced.actors.empty() || synced.actors[0] == nullptr || synced.actors[0]->update == nullptr
               ? nullptr
               : synced.actors[0];
}

void Remember(Synced& synced) {
    for (size_t i = 0; i < synced.actors.size(); i++) {
        Actor* actor = synced.actors[i];
        if (actor != nullptr) {
            synced.before[i].assign((u8*)actor, (u8*)actor + MemberDef(synced, i).size);
        } else {
            synced.before[i].clear();
        }
    }
}

nlohmann::json GroupSnapshot(Synced& synced) {
    nlohmann::json members = nlohmann::json::array();
    for (size_t i = 0; i < synced.actors.size(); i++) {
        Actor* actor = synced.actors[i];
        const StateSyncDef& def = MemberDef(synced, i);
        if (actor == nullptr || actor->update == nullptr || ActionIndex(actor, def) < 0) {
            members.push_back(nullptr);
        } else {
            members.push_back({ ActionIndex(actor, def), Base64(Snapshot(actor, def)) });
        }
    }
    return members;
}

void ApplyGroupSnapshot(Synced& synced, const nlohmann::json& members) {
    if (!members.is_array() || members.size() != synced.actors.size()) {
        return;
    }
    for (size_t i = 0; i < synced.actors.size(); i++) {
        Actor* actor = synced.actors[i];
        const StateSyncDef& def = MemberDef(synced, i);
        if (actor == nullptr || actor->update == nullptr || !members[i].is_array()) {
            continue;
        }
        int action = members[i][0].get<int>();
        std::vector<u8> data = Unbase64(members[i][1].get<std::string>());
        if (data.size() != SnapshotSize(def) ||
            (def.actionOffset != NO_ACTION && (action < 0 || action >= (int)def.fightActions.size()))) {
            continue;
        }
        // Each game grabs its own Link
        if (IsIn(def.localActions, ActionOf(actor, def)) ||
            (def.actionOffset != NO_ACTION && IsIn(def.localActions, def.fightActions[action]))) {
            continue;
        }
        std::vector<u8> before((u8*)actor, (u8*)actor + def.size);
        ApplySnapshot(actor, def, action, data);
        if (def.onApply != nullptr) {
            def.onApply(actor, before.data());
        }
    }
}

void SendSyncedPacket(const std::string& type, Key key, nlohmann::json payload) {
    payload["type"] = type;
    payload["key"] = key;
    SendToPeers(payload);
}

bool WasHit(Synced& synced) {
    for (size_t i = 0; i < synced.actors.size(); i++) {
        Actor* actor = synced.actors[i];
        const StateSyncDef& def = MemberDef(synced, i);
        if (actor == nullptr || actor->update == nullptr || synced.before[i].size() != def.size) {
            continue;
        }
        const u8* before = synced.before[i].data();
        ActionFunc action = ActionOf(actor, def);
        if ((action != ActionOf((Actor*)before, def) && IsIn(def.hitReactions, action)) ||
            ReadHealth(actor, def) < ReadHealth(before, def) || (def.wasHit != nullptr && def.wasHit(actor, before))) {
            return true;
        }
    }
    return false;
}

// What this game's frame did: a defeat goes to everyone, a hit on another player's fight goes to the room owner
void CheckSynced(Key key, Synced& synced) {
    Actor* leader = Leader(synced);
    if (leader == nullptr) {
        return;
    }
    const StateSyncDef& def = *synced.def;
    if (IsDefeated(leader, def)) {
        if (!synced.defeated) {
            synced.defeated = true;
            SendSyncedPacket(BOSS_DEFEATED, key, nlohmann::json::object());
        }
    } else if (!OwnsRoom(leader->room) && IsFighting(leader, def) && WasHit(synced)) {
        // This player hit it: the room owner takes it from here
        nlohmann::json payload;
        payload["hit"] = ++synced.hitSeq;
        payload["members"] = GroupSnapshot(synced);
        SendSyncedPacket(BOSS_HIT, key, std::move(payload));
        synced.holdUntil = sFrame + SYNC_HIT_HOLD;
    }
    Remember(synced);
}

void SendSyncedState(Key key, Synced& synced) {
    Actor* leader = Leader(synced);
    if (leader == nullptr || !OwnsRoom(leader->room) || !IsFighting(leader, *synced.def) ||
        IsDefeated(leader, *synced.def)) {
        return;
    }
    nlohmann::json payload;
    payload["seq"] = ++synced.seq;
    payload["members"] = GroupSnapshot(synced);
    if (!synced.hitsTaken.empty()) {
        nlohmann::json acks = nlohmann::json::array();
        for (auto& [clientId, hit] : synced.hitsTaken) {
            acks.push_back({ clientId, hit });
        }
        payload["acks"] = acks;
    }
    SendSyncedPacket(BOSS_STATE, key, std::move(payload));
}

void StartDefeat(Synced& synced, Actor* leader) {
    synced.pendingDefeat = false;
    if (synced.def->defeat != nullptr) {
        synced.def->defeat(leader);
    }
}

void SyncedPreUpdate(Actor* actor, Key key, const StateSyncDef& def) {
    Synced& synced = sSynced[key];
    synced.def = &def;
    FindMembers(synced, actor);
    if (synced.pendingDefeat && IsFighting(actor, def)) {
        StartDefeat(synced, actor);
    }
}

// Defeated ones are sent again now and then, for anyone who comes into the room later
void SendDefeated() {
    for (auto& [key, synced] : sSynced) {
        if (synced.defeated && IsRoomOccupied(KeyRoom(key))) {
            SendSyncedPacket(BOSS_DEFEATED, key, nlohmann::json::object());
        }
    }
}

void ForgetSyncedInEmptyRooms() {
    std::erase_if(sSynced, [](const auto& entry) {
        return Leader(entry.second) == nullptr && !IsRoomOccupied(KeyRoom(entry.first));
    });
}

// MARK: - Definitions

// clang-format off
const StateSyncDef sGohma = {
    .name = "Gohma",
    .actorId = ACTOR_BOSS_GOMA,
    .size = sizeof(BossGoma),
    .actionOffset = offsetof(BossGoma, actionFunc),
    .fightActions = { ACTION(BossGoma_FloorAttackPosture), ACTION(BossGoma_FloorPrepareAttack),
                      ACTION(BossGoma_FloorAttack), ACTION(BossGoma_FloorDamaged), ACTION(BossGoma_FloorLandStruckDown),
                      ACTION(BossGoma_FloorLand), ACTION(BossGoma_FloorStunned), ACTION(BossGoma_FallJump),
                      ACTION(BossGoma_FallStruckDown), ACTION(BossGoma_CeilingSpawnGohmas),
                      ACTION(BossGoma_CeilingPrepareSpawnGohmas), ACTION(BossGoma_FloorIdle),
                      ACTION(BossGoma_CeilingIdle), ACTION(BossGoma_FloorMain), ACTION(BossGoma_WallClimb),
                      ACTION(BossGoma_CeilingMoveToCenter) },
    .hitReactions = { ACTION(BossGoma_FloorStunned), ACTION(BossGoma_FloorDamaged), ACTION(BossGoma_FallStruckDown) },
    .deathActions = { ACTION(BossGoma_Defeated) },
    .skelAnimes = { offsetof(BossGoma, skelanime) },
    // Not the sub camera or anything only the intro and death use; not the children's states, since each player has
    // their own baby Gohmas
    .fields = { FIELD(BossGoma, frameCount), FIELD(BossGoma, patienceTimer),
                FIELDS(BossGoma, eyeLidBottomRotX, eyeIrisRotY), FIELD(BossGoma, tailLimbsScaleTimers),
                FIELDS(BossGoma, spawnGohmasActionTimer, invincibilityFrames), FIELD(BossGoma, blinkTimer),
                FIELD(BossGoma, lookedAtFrames), FIELD(BossGoma, actionState), FIELD(BossGoma, framesUntilNextAction),
                FIELD(BossGoma, timer), FIELD(BossGoma, sfxFaintTimer), FIELD(BossGoma, tailLimbsScale),
                FIELD(BossGoma, eyeIrisScaleX), FIELD(BossGoma, eyeIrisScaleY), FIELD(BossGoma, currentAnimFrameCount),
                FIELD(BossGoma, mainEnvColor), FIELD(BossGoma, eyeEnvColor) },
    .defeat = [](Actor* actor) {
        // Her own hit handler fires the defeat hook, not her death
        BossGoma_SetupDefeated((BossGoma*)actor, gPlayState);
        Enemy_StartFinishingBlow(gPlayState, actor);
        GameInteractor_ExecuteOnBossDefeat(actor);
    },
};
// clang-format on

const std::vector<const StateSyncDef*>& StateSyncDefs() {
    static const std::vector<const StateSyncDef*> defs = {
        &sGohma,
    };
    return defs;
}

} // namespace

// MARK: - Hooks

void Reset() {
    sScene = -1;
    sRemote.clear();
    sSent.clear();
    sLastHealth.clear();
    sHealthSentFrame.clear();
    sDead.clear();
    sKilledHere.clear();
    sRemoteZero.clear();
    sBlocks.clear();
    sSynced.clear();
    sDerivedKeys.clear();
}

void OnShouldActorUpdate(Actor* actor, bool* should) {
    const StateSyncDef* def = FindDef(actor);
    bool isEnemy = def == nullptr && IsSyncedEnemy(actor);
    if ((!isEnemy && def == nullptr && actor->id != ACTOR_OBJ_OSHIHIKI) || !IsEnabled()) {
        return;
    }
    CheckScene();
    DeriveKey(actor);
    Key key;
    if (!HasPeers()) {
        return;
    }
    if (def != nullptr) {
        if (!def->targetsOwnLink) {
            TargetNearestLink(actor);
        }
        if (!def->isMember && GetKey(actor, &key)) {
            SyncedPreUpdate(actor, key, *def);
        }
    } else if (!GetKey(actor, &key)) {
        return;
    } else if (isEnemy) {
        EnemyPreUpdate(actor, key, should);
    } else {
        BlockPreUpdate(actor, key);
    }
}

void OnActorUpdate(Actor* actor) {
    bool isEnemy = IsSyncedEnemy(actor);
    if ((!isEnemy && actor->id != ACTOR_OBJ_OSHIHIKI) || !IsEnabled()) {
        return;
    }
    Key key;
    if (!HasPeers() || !GetKey(actor, &key)) {
        return;
    }
    if (isEnemy) {
        EnemyPostUpdate(actor, key);
    } else {
        BlockPostUpdate(actor, key);
    }
}

void OnEnemyDefeat(Actor* actor) {
    Key key;
    if (!IsEnabled() || !IsSyncedEnemy(actor) || !GetKey(actor, &key)) {
        return;
    }
    CheckScene();
    sKilledHere.insert(key);
    if (!sDead.insert(key).second || !HasPeers()) {
        return;
    }
    nlohmann::json payload;
    payload["type"] = ACTOR_KILL;
    payload["actors"] = nlohmann::json::array({ key });
    payload["effect"] = true;
    SendToPeers(payload);
}

// A room that loads again spawns new copies of its actors
void OnActorInit(Actor* actor) {
    if (gPlayState == nullptr) {
        return;
    }
    CheckScene();
    Key key;
    if (!IsEnabled() || !GetKey(actor, &key)) {
        return;
    }
    sRemote.erase(key);
    sRemoteZero.erase(key);
    sKilledHere.erase(key);
    sLastHealth.erase(key);
    sSent.erase(key);
    sBlocks.erase(key);
    auto synced = sSynced.find(key);
    if (synced != sSynced.end()) {
        // Still defeated while anyone is in its room, like enemies stay dead
        bool defeated = synced->second.defeated;
        synced->second = Synced();
        synced->second.defeated = synced->second.pendingDefeat = defeated;
    }
}

void OnActorDestroy(Actor* actor) {
    sDerivedKeys.erase(actor);
    for (auto& [key, synced] : sSynced) {
        std::replace(synced.actors.begin(), synced.actors.end(), actor, (Actor*)nullptr);
    }
}

bool GetActorKey(Actor* actor, uint64_t* key) {
    return GetKey(actor, key);
}

void OnFrame() {
    if (!IsEnabled()) {
        return;
    }
    sFrame++;
    CheckScene();
    if (sFrame % SLOW_TICK == 0) {
        ForgetDeadInEmptyRooms();
        ForgetSyncedInEmptyRooms();
    }
    if (!HasPeers()) {
        return;
    }
    for (auto& [key, synced] : sSynced) {
        CheckSynced(key, synced);
        SendSyncedState(key, synced);
    }
    SendEnemyStates();
    if (sFrame % SLOW_TICK == 0) {
        SendBlockPositions();
        SendDeadList();
        SendDefeated();
    }
}

// MARK: - Packets

void HandlePacket_ActorState(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    uint32_t sender = payload.value("clientId", (uint32_t)0);
    for (auto& entry : payload["actors"]) {
        Key key = entry[0].get<Key>();
        // Only the room owner's state counts; anyone else is just running their own copy for a moment
        if (RoomOwner(KeyRoom(key)) != sender) {
            continue;
        }
        RemoteEnemy& remote = sRemote[key];
        remote.pos = { entry[1].get<f32>(), entry[2].get<f32>(), entry[3].get<f32>() };
        remote.shapeRotY = entry[4].get<s16>();
        remote.worldRotY = entry[5].get<s16>();
        remote.health = entry[6].get<s16>();
        remote.frame = sFrame;
    }
}

void HandlePacket_ActorHealth(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    Key key = payload["key"].get<Key>();
    s16 health = payload["health"].get<s16>();
    Actor* actor = FindActor(key);
    if (actor != nullptr && actor->update != nullptr && IsSyncedEnemy(actor) && health < actor->colChkInfo.health) {
        actor->colChkInfo.health = MAX(health, 0);
        sLastHealth[key] = actor->colChkInfo.health;
        if (actor->colChkInfo.health == 0) {
            sRemoteZero.emplace(key, sFrame);
        }
    }
}

void HandlePacket_ActorKill(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    bool withEffect = payload.value("effect", false);
    for (auto& entry : payload["actors"]) {
        Key key = entry.get<Key>();
        sDead.insert(key);
        Actor* actor = FindActor(key);
        // An enemy that got to 0 health here by itself is already playing its own death
        if (actor != nullptr && actor->update != nullptr && IsSyncedEnemy(actor) && !sKilledHere.contains(key) &&
            (actor->colChkInfo.health > 0 || sRemoteZero.contains(key))) {
            KillEnemy(actor, withEffect);
        }
    }
}

void HandlePacket_BlockPush(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    Key key = payload["key"].get<Key>();
    Actor* actor = FindActor(key);
    if (actor == nullptr || actor->id != ACTOR_OBJ_OSHIHIKI || actor->update == nullptr) {
        return;
    }
    ObjOshihiki* block = (ObjOshihiki*)actor;
    Block& tracked = TrackBlock(actor, key);
    if (IsBlockIdle(block) && tracked.pendingPushes.empty()) {
        ApplyBlockPush(block, tracked, payload);
    } else {
        tracked.pendingPushes.push_back(payload);
    }
}

// The state-synced actor a packet is about, if this game has it
static Synced* FindSynced(nlohmann::json& payload, Key* key) {
    if (!IsForThisScene(payload)) {
        return nullptr;
    }
    CheckScene();
    *key = payload["key"].get<Key>();
    auto it = sSynced.find(*key);
    return it == sSynced.end() || Leader(it->second) == nullptr ? nullptr : &it->second;
}

void HandlePacket_BossState(nlohmann::json& payload) {
    Key key;
    Synced* synced = FindSynced(payload, &key);
    uint32_t sender = payload.value("clientId", (uint32_t)0);
    if (synced == nullptr || RoomOwner(KeyRoom(key)) != sender) {
        return;
    }
    // A late packet mustn't undo a newer one
    uint32_t seq = payload.value("seq", 0u);
    if (seq <= synced->seq && sender == synced->seqFrom) {
        return;
    }
    synced->seq = seq;
    synced->seqFrom = sender;

    // After this player hit it, wait for the room owner to take the hit
    if (sFrame < synced->holdUntil) {
        uint32_t taken = 0;
        for (auto& ack : payload.value("acks", nlohmann::json::array())) {
            if (ack[0].get<uint32_t>() == Anchor::Instance->ownClientId) {
                taken = ack[1].get<uint32_t>();
            }
        }
        if (taken < synced->hitSeq) {
            return;
        }
        synced->holdUntil = 0;
    }

    // Anything this frame did goes out before the room owner's state replaces it
    CheckSynced(key, *synced);
    Actor* leader = Leader(*synced);
    if (sFrame < synced->holdUntil || !IsFighting(leader, *synced->def) || IsDefeated(leader, *synced->def)) {
        return;
    }
    ApplyGroupSnapshot(*synced, payload["members"]);
    Remember(*synced);
}

void HandlePacket_BossHit(nlohmann::json& payload) {
    Key key;
    Synced* synced = FindSynced(payload, &key);
    if (synced == nullptr || !OwnsRoom(KeyRoom(key))) {
        return;
    }
    uint32_t hit = payload.value("hit", 0u);
    uint32_t& taken = synced->hitsTaken[payload.value("clientId", (uint32_t)0)];
    if (hit <= taken) {
        return;
    }
    taken = hit;
    // The room owner carries on from the state the hit left it in
    CheckSynced(key, *synced);
    Actor* leader = Leader(*synced);
    if (IsFighting(leader, *synced->def) && !IsDefeated(leader, *synced->def)) {
        ApplyGroupSnapshot(*synced, payload["members"]);
        Remember(*synced);
    }
}

void HandlePacket_BossDefeated(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    Key key = payload["key"].get<Key>();
    Synced& synced = sSynced[key];
    if (synced.defeated) {
        return;
    }
    synced.defeated = true;
    Actor* leader = Leader(synced);
    if (leader == nullptr) {
        // Not updated here yet
        Actor* actor = FindActor(key);
        const StateSyncDef* def = actor != nullptr && actor->update != nullptr ? FindDef(actor) : nullptr;
        if (def == nullptr || def->isMember) {
            synced.pendingDefeat = true;
            return;
        }
        synced.def = def;
        FindMembers(synced, actor);
        leader = actor;
    }
    if (IsDefeated(leader, *synced.def)) {
        return;
    }
    if (IsFighting(leader, *synced.def)) {
        StartDefeat(synced, leader);
    } else {
        synced.pendingDefeat = true;
    }
}

void HandlePacket_BlockPos(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    uint32_t sender = payload.value("clientId", (uint32_t)0);
    for (auto& entry : payload["blocks"]) {
        Key key = entry[0].get<Key>();
        Actor* actor = FindActor(key);
        if (actor == nullptr || actor->id != ACTOR_OBJ_OSHIHIKI || actor->update == nullptr) {
            continue;
        }
        ObjOshihiki* block = (ObjOshihiki*)actor;
        Block& tracked = TrackBlock(actor, key);
        Vec3f pos = { entry[1].get<f32>(), entry[2].get<f32>(), entry[3].get<f32>() };
        if (!IsBlockIdle(block) || !tracked.pendingPushes.empty() ||
            sFrame - tracked.lastPushFrame < BLOCK_SETTLE_FRAMES ||
            Math3D_Vec3fDistSq(&actor->world.pos, &pos) <= 1.0f) {
            continue;
        }
        // A moved block beats one that was never touched (a player who just walked in); otherwise the room owner wins
        bool untouched = Math3D_Vec3fDistSq(&actor->world.pos, &tracked.spawnPos) <= 1.0f;
        if (!untouched && sender != RoomOwner(KeyRoom(key))) {
            continue;
        }
        actor->home.pos = pos;
        actor->world.pos = pos;
        // Lands it on whatever is under the new spot
        ObjOshihiki_SetupFall(block, gPlayState);
    }
}

// MARK: - Tests

#ifdef __EMSCRIPTEN__
// For automated browser tests (tools/web-test)
namespace {

// The collider each test hit goes to, by def name and group member
struct DebugTarget {
    const char* name;
    int member;
    size_t collider;
};

const DebugTarget sDebugTargets[] = {
    { "Gohma", 0, offsetof(BossGoma, collider) },
};

// A state-synced actor that leads its group, by its def's name
Actor* FindSyncedLeader(const char* name, const StateSyncDef** def) {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    for (int category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != nullptr; actor = actor->next) {
            const StateSyncDef* found = FindDef(actor);
            if (actor->update != nullptr && found != nullptr && !found->isMember && strcmp(found->name, name) == 0) {
                *def = found;
                return actor;
            }
        }
    }
    return nullptr;
}

Actor* FindSyncedMember(const char* name, int member, const StateSyncDef** def) {
    Actor* leader = FindSyncedLeader(name, def);
    if (leader == nullptr || member == 0) {
        return leader;
    }
    if (member > (int)(*def)->members.size()) {
        return nullptr;
    }
    Actor* actor = (*def)->members[member - 1].find(leader);
    *def = (*def)->members[member - 1].def;
    return actor != nullptr && actor->update != nullptr ? actor : nullptr;
}

nlohmann::json DebugState(Actor* actor, const StateSyncDef& def) {
    return { { "action", IsDefeated(actor, def) ? -2 : IsFighting(actor, def) ? ActionIndex(actor, def) : -1 },
             { "fighting", IsFighting(actor, def) },
             { "defeated", IsDefeated(actor, def) },
             { "health", ReadHealth(actor, def) },
             { "x", actor->world.pos.x },
             { "y", actor->world.pos.y },
             { "z", actor->world.pos.z },
             { "frame", def.skelAnimes.empty() ? 0.0f : ((SkelAnime*)((u8*)actor + def.skelAnimes[0]))->curFrame } };
}

} // namespace
#endif

} // namespace AnchorActorSync

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

using namespace AnchorActorSync;

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* web_debug_actor_sync(void) {
    static std::string out;
    nlohmann::json list = nlohmann::json::array();
    if (gPlayState != nullptr && Anchor::Instance != nullptr) {
        for (int category : { ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_BOSS }) {
            for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != nullptr;
                 actor = actor->next) {
                uint64_t key;
                if (!AnchorActorSync::GetActorKey(actor, &key) ||
                    (category == ACTORCAT_PROP && actor->id != ACTOR_OBJ_OSHIHIKI)) {
                    continue;
                }
                s16 index = (s16)(key & 0xFFFF);
                list.push_back({ { "room", actor->room },
                                 { "index", index },
                                 { "id", actor->id },
                                 { "x", actor->world.pos.x },
                                 { "y", actor->world.pos.y },
                                 { "z", actor->world.pos.z },
                                 { "rotY", actor->shape.rot.y },
                                 { "health", actor->colChkInfo.health },
                                 { "alive", actor->update != nullptr } });
            }
        }
    }
    out = list.dump();
    return out.c_str();
}

static Actor* WebDebug_FindActor(int room, int index) {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    for (int category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != nullptr; actor = actor->next) {
            uint64_t key;
            if (actor->room == room && AnchorActorSync::GetActorKey(actor, &key) && (s16)(key & 0xFFFF) == index) {
                return actor;
            }
        }
    }
    return nullptr;
}

// Every state-synced fight here: its def's name, key, owner, action (index into its fight actions; -1 when not
// fighting, -2 when defeated), health, position and animation frame, and the same for its group's members
EMSCRIPTEN_KEEPALIVE const char* web_debug_synced(void) {
    static std::string out;
    nlohmann::json list = nlohmann::json::array();
    if (gPlayState != nullptr && Anchor::Instance != nullptr) {
        for (int category = 0; category < ACTORCAT_MAX; category++) {
            for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != nullptr;
                 actor = actor->next) {
                const StateSyncDef* def = FindDef(actor);
                Key key;
                if (actor->update == nullptr || def == nullptr || def->isMember || !GetKey(actor, &key)) {
                    continue;
                }
                nlohmann::json entry = DebugState(actor, *def);
                entry["name"] = def->name;
                entry["room"] = actor->room;
                entry["index"] = (s16)(key & 0xFFFF);
                entry["owner"] = IsEnabled() && OwnsRoom(actor->room);
                nlohmann::json members = nlohmann::json::array();
                for (const GroupMember& member : def->members) {
                    Actor* found = member.find(actor);
                    members.push_back(found != nullptr && found->update != nullptr ? DebugState(found, *member.def)
                                                                                   : nullptr);
                }
                entry["members"] = members;
                list.push_back(entry);
            }
        }
    }
    out = list.dump();
    return out.c_str();
}

// Hit a state-synced actor on the next frame, as if with an attack with these damage flags (e.g. 1: Deku Nut,
// 0x100: Kokiri Sword slash). Part is the collider element. What the hit does is up to the actor's own code.
EMSCRIPTEN_KEEPALIVE int web_debug_synced_hit(const char* name, int member, int part, int dmgFlags) {
    static ColliderInfo attack;
    static Collider attacker;
    const StateSyncDef* def;
    Actor* actor = FindSyncedMember(name, member, &def);
    if (actor == nullptr) {
        return 0;
    }
    for (const DebugTarget& target : sDebugTargets) {
        if (strcmp(target.name, name) != 0 || target.member != member) {
            continue;
        }
        Collider* collider = (Collider*)((u8*)actor + target.collider);
        ColliderInfo* info = nullptr;
        switch (collider->shape) {
            case COLSHAPE_JNTSPH:
                info = part < ((ColliderJntSph*)collider)->count ? &((ColliderJntSph*)collider)->elements[part].info
                                                                  : nullptr;
                break;
            case COLSHAPE_CYLINDER:
                info = &((ColliderCylinder*)collider)->info;
                break;
            case COLSHAPE_TRIS:
                info = part < ((ColliderTris*)collider)->count ? &((ColliderTris*)collider)->elements[part].info
                                                                : nullptr;
                break;
            case COLSHAPE_QUAD:
                info = &((ColliderQuad*)collider)->info;
                break;
        }
        if (info == nullptr) {
            return 0;
        }
        attacker.actor = &GET_PLAYER(gPlayState)->actor;
        attack.toucher.dmgFlags = dmgFlags;
        attack.toucher.damage = 2;
        collider->acFlags |= AC_HIT;
        collider->ac = attacker.actor;
        info->bumperFlags |= BUMP_HIT;
        info->acHit = &attacker;
        info->acHitInfo = &attack;
        CollisionCheck_ApplyDamage(gPlayState, &gPlayState->colChkCtx, collider, info);
        return 1;
    }
    return 0;
}

// Defeat a state-synced fight here, the way a game that didn't land the blow does
EMSCRIPTEN_KEEPALIVE int web_debug_synced_defeat(const char* name) {
    const StateSyncDef* def;
    Actor* actor = FindSyncedLeader(name, &def);
    if (actor == nullptr || def->defeat == nullptr || !IsFighting(actor, *def)) {
        return 0;
    }
    def->defeat(actor);
    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_debug_synced_set_health(const char* name, int member, int health) {
    const StateSyncDef* def;
    Actor* actor = FindSyncedMember(name, member, &def);
    if (actor == nullptr) {
        return 0;
    }
    WriteHealth(actor, *def, health);
    return 1;
}

// Defeat an enemy the way its own death code does
EMSCRIPTEN_KEEPALIVE int web_debug_actor_defeat(int room, int index) {
    Actor* actor = WebDebug_FindActor(room, index);
    if (actor == nullptr || actor->update == nullptr) {
        return 0;
    }
    actor->colChkInfo.health = 0;
    GameInteractor_ExecuteOnEnemyDefeat(actor);
    Actor_Kill(actor);
    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_debug_actor_set_health(int room, int index, int health) {
    Actor* actor = WebDebug_FindActor(room, index);
    if (actor == nullptr || actor->update == nullptr) {
        return 0;
    }
    actor->colChkInfo.health = health;
    return 1;
}

// Start a push the way Link does; quarter 0-3 picks the direction
EMSCRIPTEN_KEEPALIVE int web_debug_block_push(int room, int index, int quarter) {
    Actor* actor = WebDebug_FindActor(room, index);
    if (actor == nullptr || actor->id != ACTOR_OBJ_OSHIHIKI || actor->update == nullptr) {
        return 0;
    }
    ObjOshihiki* block = (ObjOshihiki*)actor;
    if (block->actionFunc != ObjOshihiki_OnScene && block->actionFunc != ObjOshihiki_OnActor) {
        return 0;
    }
    block->dyna.unk_158 = (s16)(quarter * 0x4000);
    block->dyna.unk_150 = 1.0f;
    block->direction = 1.0f;
    block->timer = 0;
    ObjOshihiki_SetupPush(block, gPlayState);
    return 1;
}
}
#endif
