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

// A push block made by a spawner (Obj_Makeoshihiki, one block each) takes its ID from the spawner. Called on the
// block's first update: its init runs inside the spawn call, before the spawner is set as its parent.
void DeriveKey(Actor* actor) {
    if (actor->id != ACTOR_OBJ_OSHIHIKI || actor->room < 0 || GetActorListIndex(actor) >= 0 ||
        sDerivedKeys.contains(actor) || actor->parent == nullptr || actor->parent->id != ACTOR_OBJ_MAKEOSHIHIKI) {
        return;
    }
    s16 parentIndex = GetActorListIndex(actor->parent);
    if (parentIndex >= 0) {
        sDerivedKeys[actor] = MakeKey(actor->id, actor->room, parentIndex | 0x4000);
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
    if (actor->category != ACTORCAT_ENEMY) {
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

// MARK: - Bosses

/**
 * Bosses run a state machine that following a position can't keep in step, so the room owner sends the boss's
 * actual state instead: its action, timers, animation and transform. Everyone runs the boss from that state, so
 * hits land the same way for everyone; a hit that changes the boss's state on another player's game (a stun, eye
 * damage) is sent back to the room owner, who carries on from it. Intros and deaths use a camera that belongs to
 * each player's own game, so those play locally: only the fight states are shared, and a defeat starts everyone's
 * own death cutscene (with its blue warp and heart container).
 *
 * This relies on both players running the same build, which Anchor already requires: function and animation
 * pointers are the same in both.
 */
typedef void (*BossActionFunc)(void*, PlayState*);

struct BossField {
    size_t offset;
    bool isFloat; // f32, otherwise s16
};

struct BossDef {
    s16 actorId;
    bool (*match)(Actor* actor); // Which of the actors with this ID (projectiles and parts can share it); all if null
    size_t actionFuncOffset;
    size_t skelAnimeOffset;
    std::vector<BossActionFunc> fightActions;  // Sent as an index into this list
    std::vector<BossActionFunc> hitReactions; // States a hit puts the boss in
    BossActionFunc defeatedAction;
    void (*defeat)(Actor* actor);
    std::vector<BossField> fields;
};

#define BOSS_ACTION(func) ((BossActionFunc)(func))
#define BOSS_S16(type, field) BossField{ offsetof(type, field), false }
#define BOSS_F32(type, field) BossField{ offsetof(type, field), true }

const std::vector<BossDef>& BossDefs() {
    static const std::vector<BossDef> defs = {
        { ACTOR_BOSS_GOMA,
          nullptr,
          offsetof(BossGoma, actionFunc),
          offsetof(BossGoma, skelanime),
          { BOSS_ACTION(BossGoma_FloorAttackPosture), BOSS_ACTION(BossGoma_FloorPrepareAttack),
            BOSS_ACTION(BossGoma_FloorAttack), BOSS_ACTION(BossGoma_FloorDamaged),
            BOSS_ACTION(BossGoma_FloorLandStruckDown), BOSS_ACTION(BossGoma_FloorLand),
            BOSS_ACTION(BossGoma_FloorStunned), BOSS_ACTION(BossGoma_FallJump), BOSS_ACTION(BossGoma_FallStruckDown),
            BOSS_ACTION(BossGoma_CeilingSpawnGohmas), BOSS_ACTION(BossGoma_CeilingPrepareSpawnGohmas),
            BOSS_ACTION(BossGoma_FloorIdle), BOSS_ACTION(BossGoma_CeilingIdle), BOSS_ACTION(BossGoma_FloorMain),
            BOSS_ACTION(BossGoma_WallClimb), BOSS_ACTION(BossGoma_CeilingMoveToCenter) },
          { BOSS_ACTION(BossGoma_FloorStunned), BOSS_ACTION(BossGoma_FloorDamaged),
            BOSS_ACTION(BossGoma_FallStruckDown) },
          BOSS_ACTION(BossGoma_Defeated),
          [](Actor* actor) {
              BossGoma_SetupDefeated((BossGoma*)actor, gPlayState);
              Enemy_StartFinishingBlow(gPlayState, actor);
              GameInteractor_ExecuteOnBossDefeat(actor);
          },
          // Not the sub camera or anything only the intro and death use; not the children's states, since each
          // player has their own baby Gohmas
          { BOSS_S16(BossGoma, frameCount), BOSS_S16(BossGoma, patienceTimer), BOSS_S16(BossGoma, eyeLidBottomRotX),
            BOSS_S16(BossGoma, eyeLidTopRotX), BOSS_S16(BossGoma, eyeClosedTimer), BOSS_S16(BossGoma, eyeIrisRotX),
            BOSS_S16(BossGoma, eyeIrisRotY), BOSS_S16(BossGoma, tailLimbsScaleTimers[0]),
            BOSS_S16(BossGoma, tailLimbsScaleTimers[1]), BOSS_S16(BossGoma, tailLimbsScaleTimers[2]),
            BOSS_S16(BossGoma, tailLimbsScaleTimers[3]), BOSS_S16(BossGoma, spawnGohmasActionTimer),
            BOSS_S16(BossGoma, eyeState), BOSS_S16(BossGoma, doNotMoveThisFrame), BOSS_S16(BossGoma, visualState),
            BOSS_S16(BossGoma, invincibilityFrames), BOSS_S16(BossGoma, blinkTimer),
            BOSS_S16(BossGoma, lookedAtFrames), BOSS_S16(BossGoma, actionState),
            BOSS_S16(BossGoma, framesUntilNextAction), BOSS_S16(BossGoma, timer), BOSS_S16(BossGoma, sfxFaintTimer),
            BOSS_F32(BossGoma, tailLimbsScale[0]), BOSS_F32(BossGoma, tailLimbsScale[1]),
            BOSS_F32(BossGoma, tailLimbsScale[2]), BOSS_F32(BossGoma, tailLimbsScale[3]),
            BOSS_F32(BossGoma, eyeIrisScaleX), BOSS_F32(BossGoma, eyeIrisScaleY),
            BOSS_F32(BossGoma, currentAnimFrameCount), BOSS_F32(BossGoma, mainEnvColor[0]),
            BOSS_F32(BossGoma, mainEnvColor[1]), BOSS_F32(BossGoma, mainEnvColor[2]),
            BOSS_F32(BossGoma, eyeEnvColor[0]), BOSS_F32(BossGoma, eyeEnvColor[1]),
            BOSS_F32(BossGoma, eyeEnvColor[2]) } },
    };
    return defs;
}

// By ID rather than category: some change category during their fight
const BossDef* GetBossDef(Actor* actor) {
    for (const BossDef& def : BossDefs()) {
        if (def.actorId == actor->id && (def.match == nullptr || def.match(actor))) {
            return &def;
        }
    }
    return nullptr;
}

struct Boss {
    nlohmann::json state;          // Latest from the room owner, not applied yet
    uint32_t stateSeq = 0;         // Its sequence number, so a late packet doesn't undo a newer one
    uint32_t stateFrom = 0;
    uint32_t ignoreStateUntil = 0; // After sending a hit, until the room owner's state includes it
    bool pendingDefeat = false;    // Defeated elsewhere while this game was still in the intro
    BossActionFunc preAction = nullptr;
    s16 preHealth = 0;
    // As the boss's own update left it. Other actors (a reflected projectile, a part of the boss) can hit it before its
    // next update, so a change between this and the next pre-update is a hit too.
    BossActionFunc postAction = nullptr;
    s16 postHealth = 0;
    uint32_t sentSeq = 0;
};

std::unordered_map<Key, Boss> sBosses;

constexpr uint32_t BOSS_HIT_HOLD = 10;

BossActionFunc& ActionOf(Actor* actor, const BossDef& def) {
    return *(BossActionFunc*)((u8*)actor + def.actionFuncOffset);
}

SkelAnime* SkelAnimeOf(Actor* actor, const BossDef& def) {
    return (SkelAnime*)((u8*)actor + def.skelAnimeOffset);
}

int FightActionIndex(Actor* actor, const BossDef& def) {
    BossActionFunc action = ActionOf(actor, def);
    for (size_t i = 0; i < def.fightActions.size(); i++) {
        if (def.fightActions[i] == action) {
            return (int)i;
        }
    }
    return -1;
}

bool IsIn(const std::vector<BossActionFunc>& list, BossActionFunc action) {
    return std::find(list.begin(), list.end(), action) != list.end();
}

nlohmann::json BossSnapshot(Actor* actor, const BossDef& def) {
    SkelAnime* skel = SkelAnimeOf(actor, def);
    nlohmann::json fields = nlohmann::json::array();
    for (const BossField& field : def.fields) {
        if (field.isFloat) {
            fields.push_back(*(f32*)((u8*)actor + field.offset));
        } else {
            fields.push_back(*(s16*)((u8*)actor + field.offset));
        }
    }
    return { { "action", FightActionIndex(actor, def) },
             { "pos", { actor->world.pos.x, actor->world.pos.y, actor->world.pos.z } },
             { "rot", { actor->world.rot.x, actor->world.rot.y, actor->world.rot.z } },
             { "shapeRot", { actor->shape.rot.x, actor->shape.rot.y, actor->shape.rot.z } },
             { "velocity", { actor->velocity.x, actor->velocity.y, actor->velocity.z } },
             { "speedXZ", actor->speedXZ },
             { "gravity", actor->gravity },
             { "health", actor->colChkInfo.health },
             { "anim", (uint32_t)(uintptr_t)skel->animation },
             { "animMode", skel->mode },
             { "animFrames", { skel->startFrame, skel->endFrame, skel->animLength, skel->curFrame, skel->playSpeed } },
             { "fields", fields } };
}

void ApplyBossSnapshot(Actor* actor, const BossDef& def, const nlohmann::json& state) {
    int action = state["action"].get<int>();
    if (action < 0 || action >= (int)def.fightActions.size() || state["fields"].size() != def.fields.size()) {
        return;
    }
    ActionOf(actor, def) = def.fightActions[action];

    auto pos = state["pos"], rot = state["rot"], shapeRot = state["shapeRot"], velocity = state["velocity"];
    actor->world.pos = { pos[0].get<f32>(), pos[1].get<f32>(), pos[2].get<f32>() };
    actor->world.rot = { rot[0].get<s16>(), rot[1].get<s16>(), rot[2].get<s16>() };
    actor->shape.rot = { shapeRot[0].get<s16>(), shapeRot[1].get<s16>(), shapeRot[2].get<s16>() };
    actor->velocity = { velocity[0].get<f32>(), velocity[1].get<f32>(), velocity[2].get<f32>() };
    actor->speedXZ = state["speedXZ"].get<f32>();
    actor->gravity = state["gravity"].get<f32>();
    actor->colChkInfo.health = MIN(actor->colChkInfo.health, state["health"].get<u8>());

    for (size_t i = 0; i < def.fields.size(); i++) {
        if (def.fields[i].isFloat) {
            *(f32*)((u8*)actor + def.fields[i].offset) = state["fields"][i].get<f32>();
        } else {
            *(s16*)((u8*)actor + def.fields[i].offset) = state["fields"][i].get<s16>();
        }
    }

    // A different animation blends in from the current pose, like a local animation change would
    SkelAnime* skel = SkelAnimeOf(actor, def);
    char* anim = (char*)(uintptr_t)state["anim"].get<uint32_t>();
    u8 mode = state["animMode"].get<u8>();
    if ((anim != skel->animation || mode != skel->mode) && ResourceMgr_OTRSigCheck(anim)) {
        skel->animation = anim;
        skel->mode = mode;
        SkelAnime_SetUpdate(skel);
    }
    auto frames = state["animFrames"];
    skel->startFrame = frames[0].get<f32>();
    skel->endFrame = frames[1].get<f32>();
    skel->animLength = frames[2].get<f32>();
    skel->curFrame = frames[3].get<f32>();
    skel->playSpeed = frames[4].get<f32>();
}

void SendBossPacket(const std::string& type, Key key, nlohmann::json state) {
    nlohmann::json payload;
    payload["type"] = type;
    payload["key"] = key;
    payload["state"] = std::move(state);
    SendToPeers(payload);
}

bool WasHit(Actor* actor, const BossDef& def, BossActionFunc before, s16 healthBefore) {
    BossActionFunc action = ActionOf(actor, def);
    return (action != before && IsIn(def.hitReactions, action)) || actor->colChkInfo.health < healthBefore;
}

void SendBossHit(Actor* actor, Key key, const BossDef& def, Boss& boss) {
    // This player hit it: the room owner takes it from here
    SendBossPacket(BOSS_HIT, key, BossSnapshot(actor, def));
    boss.ignoreStateUntil = sFrame + BOSS_HIT_HOLD;
    boss.state = nullptr;
}

void BossPreUpdate(Actor* actor, Key key, const BossDef& def) {
    Boss& boss = sBosses[key];
    bool fighting = FightActionIndex(actor, def) >= 0;
    bool ownsRoom = OwnsRoom(actor->room);
    TargetNearestLink(actor);

    if (fighting && !ownsRoom && boss.postAction != nullptr && WasHit(actor, def, boss.postAction, boss.postHealth)) {
        SendBossHit(actor, key, def, boss);
    }
    if (boss.pendingDefeat && fighting) {
        boss.pendingDefeat = false;
        def.defeat(actor);
    } else if (fighting && !ownsRoom && !boss.state.is_null() && sFrame >= boss.ignoreStateUntil) {
        // Once: the boss runs by itself until the next one, so animation events don't fire twice
        ApplyBossSnapshot(actor, def, boss.state);
        boss.state = nullptr;
    }
    boss.preAction = ActionOf(actor, def);
    boss.preHealth = actor->colChkInfo.health;
}

void BossPostUpdate(Actor* actor, Key key, const BossDef& def) {
    Boss& boss = sBosses[key];
    BossActionFunc action = ActionOf(actor, def);
    boss.postAction = action;
    boss.postHealth = actor->colChkInfo.health;

    if (action == def.defeatedAction) {
        if (boss.preAction != def.defeatedAction) {
            SendBossPacket(BOSS_DEFEATED, key, nullptr);
        }
        return;
    }
    if (FightActionIndex(actor, def) < 0) {
        return; // Intro
    }
    if (OwnsRoom(actor->room)) {
        nlohmann::json state = BossSnapshot(actor, def);
        state["seq"] = ++boss.sentSeq;
        SendBossPacket(BOSS_STATE, key, std::move(state));
    } else if (WasHit(actor, def, boss.preAction, boss.preHealth)) {
        SendBossHit(actor, key, def, boss);
    }
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
    sBosses.clear();
    sDerivedKeys.clear();
}

void OnShouldActorUpdate(Actor* actor, bool* should) {
    bool isEnemy = IsSyncedEnemy(actor);
    const BossDef* boss = isEnemy ? nullptr : GetBossDef(actor);
    if ((!isEnemy && boss == nullptr && actor->id != ACTOR_OBJ_OSHIHIKI) || !IsEnabled()) {
        return;
    }
    CheckScene();
    DeriveKey(actor);
    Key key;
    if (!HasPeers() || !GetKey(actor, &key)) {
        return;
    }
    if (isEnemy) {
        EnemyPreUpdate(actor, key, should);
    } else if (boss != nullptr) {
        BossPreUpdate(actor, key, *boss);
    } else {
        BlockPreUpdate(actor, key);
    }
}

void OnActorUpdate(Actor* actor) {
    bool isEnemy = IsSyncedEnemy(actor);
    const BossDef* boss = isEnemy ? nullptr : GetBossDef(actor);
    if ((!isEnemy && boss == nullptr && actor->id != ACTOR_OBJ_OSHIHIKI) || !IsEnabled()) {
        return;
    }
    Key key;
    if (!HasPeers() || !GetKey(actor, &key)) {
        return;
    }
    if (isEnemy) {
        EnemyPostUpdate(actor, key);
    } else if (boss != nullptr) {
        BossPostUpdate(actor, key, *boss);
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
    sBosses.erase(key);
}

void OnActorDestroy(Actor* actor) {
    sDerivedKeys.erase(actor);
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
    }
    if (!HasPeers()) {
        return;
    }
    SendEnemyStates();
    if (sFrame % SLOW_TICK == 0) {
        SendBlockPositions();
        SendDeadList();
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

// Finds the boss a packet is about, if this game has it
static Actor* FindBoss(nlohmann::json& payload, Key* key, const BossDef** def) {
    if (!IsForThisScene(payload)) {
        return nullptr;
    }
    CheckScene();
    *key = payload["key"].get<Key>();
    Actor* actor = FindActor(*key);
    if (actor == nullptr || actor->update == nullptr || (*def = GetBossDef(actor)) == nullptr) {
        return nullptr;
    }
    return actor;
}

void HandlePacket_BossState(nlohmann::json& payload) {
    Key key;
    const BossDef* def;
    Actor* actor = FindBoss(payload, &key, &def);
    if (actor == nullptr || RoomOwner(KeyRoom(key)) != payload.value("clientId", (uint32_t)0)) {
        return;
    }
    Boss& boss = sBosses[key];
    uint32_t seq = payload["state"].value("seq", 0u);
    uint32_t sender = payload.value("clientId", (uint32_t)0);
    if (seq > boss.stateSeq || sender != boss.stateFrom) {
        boss.stateSeq = seq;
        boss.stateFrom = sender;
        boss.state = payload["state"];
    }
}

void HandlePacket_BossHit(nlohmann::json& payload) {
    Key key;
    const BossDef* def;
    Actor* actor = FindBoss(payload, &key, &def);
    // The room owner carries on from the state the hit left the boss in
    if (actor != nullptr && OwnsRoom(KeyRoom(key)) && FightActionIndex(actor, *def) >= 0) {
        ApplyBossSnapshot(actor, *def, payload["state"]);
    }
}

void HandlePacket_BossDefeated(nlohmann::json& payload) {
    Key key;
    const BossDef* def;
    Actor* actor = FindBoss(payload, &key, &def);
    if (actor == nullptr || ActionOf(actor, *def) == def->defeatedAction) {
        return;
    }
    if (FightActionIndex(actor, *def) >= 0) {
        sBosses[key].preAction = def->defeatedAction; // Don't send it back
        def->defeat(actor);
    } else {
        sBosses[key].pendingDefeat = true;
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

} // namespace AnchorActorSync

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// For automated browser tests (tools/web-test)
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

static BossGoma* WebDebug_FindGohma() {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != nullptr; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_GOMA && actor->update != nullptr) {
            return (BossGoma*)actor;
        }
    }
    return nullptr;
}

// Gohma's action (index into the synced fight actions, -1 for the intro, -2 when defeated), position and health
EMSCRIPTEN_KEEPALIVE const char* web_debug_gohma(void) {
    static std::string out;
    BossGoma* goma = WebDebug_FindGohma();
    if (goma == nullptr) {
        return "null";
    }
    static const void* actions[] = { (void*)BossGoma_FloorAttackPosture, (void*)BossGoma_FloorPrepareAttack,
        (void*)BossGoma_FloorAttack, (void*)BossGoma_FloorDamaged, (void*)BossGoma_FloorLandStruckDown,
        (void*)BossGoma_FloorLand, (void*)BossGoma_FloorStunned, (void*)BossGoma_FallJump,
        (void*)BossGoma_FallStruckDown, (void*)BossGoma_CeilingSpawnGohmas,
        (void*)BossGoma_CeilingPrepareSpawnGohmas, (void*)BossGoma_FloorIdle, (void*)BossGoma_CeilingIdle,
        (void*)BossGoma_FloorMain, (void*)BossGoma_WallClimb, (void*)BossGoma_CeilingMoveToCenter };
    int action = goma->actionFunc == BossGoma_Defeated ? -2 : -1;
    for (int i = 0; i < (int)ARRAY_COUNT(actions); i++) {
        if ((void*)goma->actionFunc == actions[i]) {
            action = i;
        }
    }
    out = nlohmann::json({ { "action", action },
                           { "x", goma->actor.world.pos.x },
                           { "y", goma->actor.world.pos.y },
                           { "z", goma->actor.world.pos.z },
                           { "frame", goma->skelanime.curFrame },
                           { "health", goma->actor.colChkInfo.health } })
              .dump();
    return out.c_str();
}

// Hit Gohma's eye on the next frame, as if with an attack with these damage flags (1: Deku Nut, 0x100: Kokiri Sword
// slash). Whether it stuns or hurts her is up to her own code, as for a real hit.
EMSCRIPTEN_KEEPALIVE int web_debug_gohma_hit(int dmgFlags) {
    static ColliderInfo attack;
    BossGoma* goma = WebDebug_FindGohma();
    if (goma == nullptr) {
        return 0;
    }
    attack.toucher.dmgFlags = dmgFlags;
    goma->collider.elements[0].info.acHitInfo = &attack;
    goma->collider.elements[0].info.bumperFlags |= BUMP_HIT;
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
