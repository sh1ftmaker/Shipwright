#include "ActorSync.h"
#include "Anchor.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <cmath>
#include <climits>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"
extern PlayState* gPlayState;
void ObjOshihiki_OnScene(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_OnActor(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_Push(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_SetupPush(ObjOshihiki* block, PlayState* play);
void ObjOshihiki_SetupFall(ObjOshihiki* block, PlayState* play);
}

namespace AnchorActorSync {

static const std::string ACTOR_STATE = "ACTOR_STATE";
static const std::string ACTOR_HEALTH = "ACTOR_HEALTH";
static const std::string ACTOR_KILL = "ACTOR_KILL";
static const std::string BLOCK_PUSH = "BLOCK_PUSH";
static const std::string BLOCK_POS = "BLOCK_POS";

namespace {

// All in game frames, 20 per second
constexpr uint32_t STATE_TIMEOUT = 40;        // A room owner's enemy state stays in charge this long
constexpr uint32_t STATE_REFRESH = 10;        // Resend enemies that haven't changed this often
constexpr uint32_t HEALTH_RESEND = 20;        // Remind the room owner about damage it hasn't applied
constexpr uint32_t SLOW_TICK = 40;            // Moved blocks and dead enemies are resent this often
constexpr uint32_t BLOCK_SETTLE_FRAMES = 60;  // Ignore block positions this long after a push

// Room in the high half, spawn list index in the low half
typedef uint32_t Key;

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
std::unordered_set<Key> sDead;
std::unordered_map<Key, Block> sBlocks;

Key MakeKey(s8 room, s16 index) {
    return ((uint32_t)(u8)room << 16) | (u16)index;
}

s8 KeyRoom(Key key) {
    return (s8)(key >> 16);
}

s16 KeyIndex(Key key) {
    return (s16)(key & 0xFFFF);
}

// Only actors from a room's spawn list have an ID every client agrees on
bool GetKey(Actor* actor, Key* key) {
    if (actor->room < 0) {
        return false;
    }
    s16 index = GetActorListIndex(actor);
    if (index < 0) {
        return false;
    }
    *key = MakeKey(actor->room, index);
    return true;
}

bool IsEnabled() {
    Anchor* anchor = Anchor::Instance;
    return anchor != nullptr && anchor->isConnected && anchor->roomState.syncActors && anchor->IsSaveLoaded();
}

bool IsPeer(const AnchorClient& client) {
    return !client.self && client.online && client.isSaveLoaded && client.actorSync &&
           client.sceneNum == gPlayState->sceneNum && client.clientVersion == Anchor::clientVersion;
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
            if (actor->room == KeyRoom(key) && GetActorListIndex(actor) == KeyIndex(key)) {
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
        actor->colChkInfo.health = remote.health;
        sLastHealth[key] = remote.health;
    }
}

void SendHealth(Key key, s16 health) {
    sHealthSentFrame[key] = sFrame;
    nlohmann::json payload;
    payload["type"] = ACTOR_HEALTH;
    payload["room"] = KeyRoom(key);
    payload["index"] = KeyIndex(key);
    payload["health"] = health;
    SendToPeers(payload);
}

void EnemyPreUpdate(Actor* actor, Key key, bool* should) {
    if (sDead.contains(key)) {
        // Someone already killed it this visit
        Actor_Kill(actor);
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
        list.push_back({ KeyRoom(key), KeyIndex(key), now.pos.x, now.pos.y, now.pos.z, now.shapeRotY, now.worldRotY,
                         now.health });
    }

    if (!list.empty()) {
        nlohmann::json payload;
        payload["type"] = ACTOR_STATE;
        payload["actors"] = list;
        SendToPeers(payload);
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

void SendDeadList() {
    if (sDead.empty()) {
        return;
    }
    nlohmann::json list = nlohmann::json::array();
    for (Key key : sDead) {
        list.push_back({ KeyRoom(key), KeyIndex(key) });
    }
    nlohmann::json payload;
    payload["type"] = ACTOR_KILL;
    payload["actors"] = list;
    payload["effect"] = false;
    SendToPeers(payload);
}

// Enemies respawn when their room loads again, like in single player, once nobody is left in it
void ForgetDeadInEmptyRooms() {
    for (auto it = sDead.begin(); it != sDead.end();) {
        if (!IsRoomOccupied(KeyRoom(*it))) {
            it = sDead.erase(it);
        } else {
            ++it;
        }
    }
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
            payload["room"] = KeyRoom(key);
            payload["index"] = KeyIndex(key);
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
        list.push_back({ KeyRoom(key), KeyIndex(key), actor->world.pos.x, actor->world.pos.y, actor->world.pos.z });
    }
    if (!list.empty()) {
        nlohmann::json payload;
        payload["type"] = BLOCK_POS;
        payload["blocks"] = list;
        SendToPeers(payload);
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
    sBlocks.clear();
}

void OnShouldActorUpdate(Actor* actor, bool* should) {
    bool isEnemy = IsSyncedEnemy(actor);
    if ((!isEnemy && actor->id != ACTOR_OBJ_OSHIHIKI) || !IsEnabled()) {
        return;
    }
    CheckScene();
    Key key;
    if (!HasPeers() || !GetKey(actor, &key)) {
        return;
    }
    if (isEnemy) {
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
    if (!sDead.insert(key).second || !HasPeers()) {
        return;
    }
    nlohmann::json payload;
    payload["type"] = ACTOR_KILL;
    payload["actors"] = nlohmann::json::array({ { KeyRoom(key), KeyIndex(key) } });
    payload["effect"] = true;
    SendToPeers(payload);
}

// A room that loads again spawns new copies of its actors
void OnActorInit(Actor* actor) {
    Key key;
    if (!IsEnabled() || !GetKey(actor, &key)) {
        return;
    }
    sRemote.erase(key);
    sLastHealth.erase(key);
    sSent.erase(key);
    sBlocks.erase(key);
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
        Key key = MakeKey(entry[0].get<s8>(), entry[1].get<s16>());
        // Only the room owner's state counts; anyone else is just running their own copy for a moment
        if (RoomOwner(KeyRoom(key)) != sender) {
            continue;
        }
        RemoteEnemy& remote = sRemote[key];
        remote.pos = { entry[2].get<f32>(), entry[3].get<f32>(), entry[4].get<f32>() };
        remote.shapeRotY = entry[5].get<s16>();
        remote.worldRotY = entry[6].get<s16>();
        remote.health = entry[7].get<s16>();
        remote.frame = sFrame;
    }
}

void HandlePacket_ActorHealth(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    Key key = MakeKey(payload["room"].get<s8>(), payload["index"].get<s16>());
    s16 health = payload["health"].get<s16>();
    Actor* actor = FindActor(key);
    if (actor != nullptr && actor->update != nullptr && IsSyncedEnemy(actor) && health < actor->colChkInfo.health) {
        actor->colChkInfo.health = MAX(health, 0);
        sLastHealth[key] = actor->colChkInfo.health;
    }
}

void HandlePacket_ActorKill(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    bool withEffect = payload.value("effect", false);
    for (auto& entry : payload["actors"]) {
        Key key = MakeKey(entry[0].get<s8>(), entry[1].get<s16>());
        sDead.insert(key);
        Actor* actor = FindActor(key);
        // An enemy at 0 health is already playing its own death here
        if (actor != nullptr && actor->update != nullptr && IsSyncedEnemy(actor) && actor->colChkInfo.health > 0) {
            KillEnemy(actor, withEffect);
        }
    }
}

void HandlePacket_BlockPush(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    Key key = MakeKey(payload["room"].get<s8>(), payload["index"].get<s16>());
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

void HandlePacket_BlockPos(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    uint32_t sender = payload.value("clientId", (uint32_t)0);
    for (auto& entry : payload["blocks"]) {
        Key key = MakeKey(entry[0].get<s8>(), entry[1].get<s16>());
        Actor* actor = FindActor(key);
        if (actor == nullptr || actor->id != ACTOR_OBJ_OSHIHIKI || actor->update == nullptr) {
            continue;
        }
        ObjOshihiki* block = (ObjOshihiki*)actor;
        Block& tracked = TrackBlock(actor, key);
        Vec3f pos = { entry[2].get<f32>(), entry[3].get<f32>(), entry[4].get<f32>() };
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
        for (int category : { ACTORCAT_ENEMY, ACTORCAT_PROP }) {
            for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != nullptr;
                 actor = actor->next) {
                s16 index = GetActorListIndex(actor);
                if (actor->room < 0 || index < 0 ||
                    (category == ACTORCAT_PROP && actor->id != ACTOR_OBJ_OSHIHIKI)) {
                    continue;
                }
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
            if (actor->room == room && GetActorListIndex(actor) == index) {
                return actor;
            }
        }
    }
    return nullptr;
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
