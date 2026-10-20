#include "Anchor.h"
#include "WorldSync.h"
#include "soh/Enhancements/game-interactor/GameInteractionEffect.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include <chrono>
#include <climits>
#include <deque>
#include <unordered_map>
#include <unordered_set>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "src/overlays/actors/ovl_Bg_Ddan_Kd/z_bg_ddan_kd.h"
#include "src/overlays/actors/ovl_Bg_Heavy_Block/z_bg_heavy_block.h"
#include "src/overlays/actors/ovl_Bg_Jya_Megami/z_bg_jya_megami.h"
#include "src/overlays/actors/ovl_En_G_Switch/z_en_g_switch.h"
#include "src/overlays/actors/ovl_Obj_Bean/z_obj_bean.h"
#include "src/overlays/actors/ovl_Obj_Switch/z_obj_switch.h"
#include "src/overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
extern PlayState* gPlayState;
void BgDdanKd_CheckForExplosions(BgDdanKd* bgDdanKd, PlayState* play);
void BgDdanKd_LowerStairs(BgDdanKd* bgDdanKd, PlayState* play);
void BgHeavyBlock_Wait(BgHeavyBlock* bgHeavyBlock, PlayState* play);
void BgHeavyBlock_SpawnPieces(BgHeavyBlock* bgHeavyBlock, PlayState* play);
void BgJyaMegami_DetectLight(BgJyaMegami* bgJyaMegami, PlayState* play);
void EnGSwitch_SilverRupeeIdle(EnGSwitch* enGSwitch, PlayState* play);
void EnGSwitch_SilverRupeeCollected(EnGSwitch* enGSwitch, PlayState* play);
void ObjBean_WaitForBean(ObjBean* objBean, PlayState* play);
void func_80B8FE6C(ObjBean* objBean);
void ObjSwitch_FloorUp(ObjSwitch* objSwitch, PlayState* play);
void ObjSwitch_FloorPressInit(ObjSwitch* objSwitch);
void ObjSwitch_FloorDown(ObjSwitch* objSwitch, PlayState* play);
void ObjSwitch_FloorReleaseInit(ObjSwitch* objSwitch);
void ObjSwitch_EyeOpen(ObjSwitch* objSwitch, PlayState* play);
void ObjSwitch_EyeClosingInit(ObjSwitch* objSwitch);
void ObjSwitch_EyeClosed(ObjSwitch* objSwitch, PlayState* play);
void ObjSwitch_EyeOpeningInit(ObjSwitch* objSwitch);
}

namespace AnchorWorldSync {

// An item reaches OnItemReceive a few seconds after its chest opened or its collectible was touched (the get-item
// animation), so a source flag set up to this many frames earlier still counts
constexpr u32 ITEM_SOURCE_FRAMES = 200;
// How long we remember having claimed an item ourselves, for the GS tokens whose flag is set only after the text box
constexpr u32 LOCAL_CLAIM_FRAMES = 600;

static std::unordered_set<uint32_t> sSetByTeammate;

struct TimedRef {
    FlagRef ref;
    u32 frame = 0;
};
static TimedRef sItemSource;  // latest chest/collectible flag we set
static TimedRef sSwitchSource; // latest switch flag we set (doors set theirs in the same frame they take the key)
static u32 sJunkFrame = UINT32_MAX;
static std::deque<TimedRef> sLocalClaims;

static u8 sItems[ARRAY_COUNT(gSaveContext.inventory.items)];
static s8 sKeys[ARRAY_COUNT(gSaveContext.inventory.dungeonKeys)];
static s8 sBeans;

static u32 Frame() {
    return gPlayState != nullptr ? gPlayState->state.frames : 0;
}

static uint32_t RefKey(const FlagRef& ref) {
    return ((uint32_t)(u16)ref.sceneNum << 16) | ((uint32_t)(u8)ref.flagType << 8) | (u8)ref.flag;
}

static bool SameRef(const FlagRef& a, const FlagRef& b) {
    return a.sceneNum == b.sceneNum && a.flagType == b.flagType && a.flag == b.flag;
}

void WriteSource(nlohmann::json& payload, const FlagRef& source) {
    if (source.IsValid()) {
        payload["source"] = { source.sceneNum, source.flagType, source.flag };
    }
}

FlagRef ReadSource(const nlohmann::json& payload) {
    FlagRef ref;
    if (payload.contains("source") && payload["source"].is_array() && payload["source"].size() == 3) {
        ref.sceneNum = payload["source"][0].get<s16>();
        ref.flagType = payload["source"][1].get<s16>();
        ref.flag = payload["source"][2].get<s16>();
        if (ref.sceneNum > SCENE_ID_MAX || ref.flag < 0 || ref.flag >= 0x40 * 32) {
            ref.sceneNum = -1;
        }
    }
    return ref;
}

bool IsPerPlayerFlag(s16 sceneNum, s16 flagType, s16 flag) {
    switch (flagType) {
        case FLAG_EVENT_INF:
            // Minigame, race and horse state of whoever is playing it
            return true;
        case FLAG_INF_TABLE:
            return flag == INFTABLE_SWORDLESS;
        case FLAG_SCENE_SWITCH:
            switch (sceneNum) {
                case SCENE_WATER_TEMPLE:
                    // Water level. Bg_Mizu_Water follows these every frame, so a shared level would flood or drain
                    // the rooms under a teammate
                    return flag == 0x1C || flag == 0x1D || flag == 0x1E;
                case SCENE_FOREST_TEMPLE:
                    // Elevator position
                    return flag == 0x1B;
                case SCENE_GANONS_TOWER_COLLAPSE_INTERIOR:
                case SCENE_INSIDE_GANONS_CASTLE_COLLAPSE:
                case SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR:
                    // Collapse timer start (En_Zl3 sets it with the timer, En_Eg voids out when it runs out)
                    return flag == 0x36;
                case SCENE_GREAT_FAIRYS_FOUNTAIN_MAGIC:
                case SCENE_GREAT_FAIRYS_FOUNTAIN_SPELLS:
                    // Great Fairy cutscene
                    return flag == 0x38;
            }
            return false;
    }
    return false;
}

bool IsRoomLocalFlag(s16 flagType, s16 flag) {
    return flagType == FLAG_SCENE_SWITCH && flag >= 0x38;
}

bool IsTempFlag(s16 sceneNum, s16 flagType, s16 flag) {
    return sceneNum != SCENE_ID_MAX && (flagType == FLAG_SCENE_SWITCH || flagType == FLAG_SCENE_COLLECTIBLE) &&
           flag >= 0x20;
}

static bool InCurrentScene(s16 sceneNum) {
    return gPlayState != nullptr && gPlayState->sceneNum == sceneNum;
}

bool IsFlagSet(const FlagRef& ref) {
    if (!ref.IsValid()) {
        return false;
    }
    if (ref.sceneNum == SCENE_ID_MAX) {
        switch (ref.flagType) {
            case FLAG_GS_TOKEN:
                return (GET_GS_FLAGS((ref.flag & 0x1F00) >> 8) & (ref.flag & 0xFF)) != 0;
            case FLAG_EVENT_CHECK_INF:
                return Flags_GetEventChkInf(ref.flag);
            case FLAG_ITEM_GET_INF:
                return Flags_GetItemGetInf(ref.flag);
            case FLAG_INF_TABLE:
                return Flags_GetInfTable(ref.flag);
            case FLAG_RANDOMIZER_INF:
                return Flags_GetRandomizerInf((RandomizerInf)ref.flag);
        }
        return false;
    }

    // The current scene's flags live in the actor context until the scene is left
    bool current = InCurrentScene(ref.sceneNum);
    const SavedSceneFlags& saved = gSaveContext.sceneFlags[ref.sceneNum];
    switch (ref.flagType) {
        case FLAG_SCENE_SWITCH:
            if (current) {
                return Flags_GetSwitch(gPlayState, ref.flag);
            }
            return ref.flag < 0x20 && (saved.swch & (1u << ref.flag));
        case FLAG_SCENE_TREASURE:
            return current ? Flags_GetTreasure(gPlayState, ref.flag) : (saved.chest & (1u << ref.flag));
        case FLAG_SCENE_CLEAR:
            return current ? Flags_GetClear(gPlayState, ref.flag) : (saved.clear & (1u << ref.flag));
        case FLAG_SCENE_COLLECTIBLE:
            if (current) {
                return Flags_GetCollectible(gPlayState, ref.flag);
            }
            return ref.flag < 0x20 && (saved.collect & (1u << ref.flag));
    }
    return false;
}

void MarkSetByTeammate(const FlagRef& ref) {
    sSetByTeammate.insert(RefKey(ref));
}

void SetFlagFromTeammate(const FlagRef& ref) {
    if (!ref.IsValid() || IsFlagSet(ref)) {
        return;
    }
    MarkSetByTeammate(ref);
    if (ref.sceneNum == SCENE_ID_MAX) {
        GameInteractionEffect::SetFlag effect;
        effect.parameters[0] = ref.flagType;
        effect.parameters[1] = ref.flag;
        effect.Apply();
    } else {
        GameInteractionEffect::SetSceneFlag effect;
        effect.parameters[0] = ref.sceneNum;
        effect.parameters[1] = ref.flagType;
        effect.parameters[2] = ref.flag;
        effect.Apply();
    }
}

bool ClaimedLocally(const FlagRef& ref) {
    if (!ref.IsValid()) {
        return false;
    }
    for (auto& claim : sLocalClaims) {
        if (SameRef(claim.ref, ref)) {
            return true;
        }
    }
    return IsFlagSet(ref) && !sSetByTeammate.contains(RefKey(ref));
}

void Reset() {
    sSetByTeammate.clear();
    sLocalClaims.clear();
    sItemSource = {};
    sSwitchSource = {};
    RefreshSnapshot();
}

void OnLocalFlagSet(s16 sceneNum, s16 flagType, s16 flag) {
    // The hook also fires for the flags we apply from teammates' packets, when they go through Flags_*
    sSetByTeammate.erase(RefKey({ sceneNum, flagType, flag }));

    u32 frame = Frame();
    switch (flagType) {
        case FLAG_SCENE_TREASURE:
            sItemSource = { { sceneNum, flagType, flag }, frame };
            break;
        case FLAG_SCENE_COLLECTIBLE:
            // Rupees and hearts set their collectible flag right after giving themselves; those aren't sources
            if (flag != 0 && frame != sJunkFrame) {
                sItemSource = { { sceneNum, flagType, flag }, frame };
            }
            break;
        case FLAG_SCENE_SWITCH:
            sSwitchSource = { { sceneNum, flagType, flag }, frame };
            break;
    }
}

static void Claim(const FlagRef& ref) {
    u32 frame = Frame();
    while (!sLocalClaims.empty() && frame - sLocalClaims.front().frame > LOCAL_CLAIM_FRAMES) {
        sLocalClaims.pop_front();
    }
    sLocalClaims.push_back({ ref, frame });
}

// The Gold Skulltula token being collected: En_Si gives the item first and sets its flag after the text box
static FlagRef FindSkulltulaToken() {
    if (gPlayState == nullptr) {
        return {};
    }
    Player* player = GET_PLAYER(gPlayState);
    Actor* best = nullptr;
    f32 bestDist = 300.0f;
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; actor != nullptr;
         actor = actor->next) {
        if (actor->id == ACTOR_EN_SI && actor->update != nullptr) {
            f32 dist = Actor_WorldDistXYZToActor(actor, &player->actor);
            if (dist < bestDist) {
                bestDist = dist;
                best = actor;
            }
        }
    }
    if (best == nullptr) {
        return {};
    }
    return { SCENE_ID_MAX, FLAG_GS_TOKEN, best->params };
}

FlagRef TakeItemSource(GetItemEntry itemEntry) {
    FlagRef source;
    if (itemEntry.itemId == ITEM_SKULL_TOKEN) {
        source = FindSkulltulaToken();
    } else if (sItemSource.ref.IsValid() && Frame() - sItemSource.frame <= ITEM_SOURCE_FRAMES &&
               InCurrentScene(sItemSource.ref.sceneNum)) {
        source = sItemSource.ref;
    }
    sItemSource = {};
    if (source.IsValid()) {
        Claim(source);
    }
    return source;
}

FlagRef TakeDoorSource() {
    FlagRef source;
    if (sSwitchSource.ref.IsValid() && sSwitchSource.frame == Frame()) {
        source = sSwitchSource.ref;
        Claim(source);
    }
    sSwitchSource = {};
    return source;
}

// The item whose slot an ammo pickup fills
static s32 AmmoBaseItem(s32 itemId) {
    switch (itemId) {
        case ITEM_STICK:
        case ITEM_STICKS_5:
        case ITEM_STICKS_10:
            return ITEM_STICK;
        case ITEM_NUT:
        case ITEM_NUTS_5:
        case ITEM_NUTS_10:
            return ITEM_NUT;
        case ITEM_BOMB:
        case ITEM_BOMBS_5:
        case ITEM_BOMBS_10:
        case ITEM_BOMBS_20:
        case ITEM_BOMBS_30:
            return ITEM_BOMB;
        case ITEM_ARROWS_SMALL:
        case ITEM_ARROWS_MEDIUM:
        case ITEM_ARROWS_LARGE:
            return ITEM_BOW;
        case ITEM_SEEDS:
        case ITEM_SEEDS_30:
            return ITEM_SLINGSHOT;
        case ITEM_BOMBCHU:
        case ITEM_BOMBCHUS_5:
        case ITEM_BOMBCHUS_20:
            return ITEM_BOMBCHU;
    }
    return ITEM_NONE;
}

bool IsConsumable(GetItemEntry itemEntry) {
    bool consumable = false;
    if (itemEntry.modIndex == MOD_RANDOMIZER &&
        (itemEntry.getItemId == RG_ICE_TRAP || itemEntry.getItemCategory != ITEM_CATEGORY_JUNK)) {
        // Rando bottles with contents and the like reuse these item IDs
        consumable = false;
    } else {
        switch (itemEntry.itemId) {
            case ITEM_RUPEE_GREEN:
            case ITEM_RUPEE_BLUE:
            case ITEM_RUPEE_RED:
            case ITEM_RUPEE_PURPLE:
            case ITEM_RUPEE_GOLD:
            case ITEM_HEART:
            case ITEM_MAGIC_SMALL:
            case ITEM_MAGIC_LARGE:
            // Bottle contents (from shops, fishing, catching), not the bottles themselves
            case ITEM_POTION_RED:
            case ITEM_POTION_GREEN:
            case ITEM_POTION_BLUE:
            case ITEM_FAIRY:
            case ITEM_FISH:
            case ITEM_MILK:
            case ITEM_MILK_HALF:
            case ITEM_BLUE_FIRE:
            case ITEM_BUG:
            case ITEM_POE:
            case ITEM_BIG_POE:
                consumable = true;
                break;
            default: {
                s32 base = AmmoBaseItem(itemEntry.itemId);
                // The first one unlocks the slot, that's progress
                consumable = base != ITEM_NONE && sItems[SLOT(base)] == base;
                break;
            }
        }
    }
    if (consumable) {
        sJunkFrame = Frame();
    }
    return consumable;
}

void RefreshSnapshot() {
    memcpy(sItems, gSaveContext.inventory.items, sizeof(sItems));
    memcpy(sKeys, gSaveContext.inventory.dungeonKeys, sizeof(sKeys));
    sBeans = AMMO(ITEM_BEAN);
}

s8 TakeKeysSpent(u16 mapIndex) {
    if (mapIndex >= ARRAY_COUNT(sKeys)) {
        return 0;
    }
    s8 before = MAX(sKeys[mapIndex], 0);
    s8 now = MAX(gSaveContext.inventory.dungeonKeys[mapIndex], 0);
    sKeys[mapIndex] = gSaveContext.inventory.dungeonKeys[mapIndex];
    return now < before ? before - now : 0;
}

void ApplyKeyDelta(u16 mapIndex, s8 delta) {
    if (mapIndex >= ARRAY_COUNT(sKeys)) {
        return;
    }
    s8& keys = gSaveContext.inventory.dungeonKeys[mapIndex];
    keys = (s8)CLAMP(MAX(keys, 0) + delta, 0, 99);
    sKeys[mapIndex] = keys;
}

bool TakeBeanPlanted() {
    s8 now = AMMO(ITEM_BEAN);
    bool planted = now < sBeans;
    sBeans = now;
    return planted;
}

// MARK: - Live world

/**
 * WORLD_EVENT
 *
 * Changes to things in a room that no flag records until they're finished: a torch of a group lit (the group's flag
 * is only set when all of them burn) and a silver rupee collected (the puzzle's flag is only set by the last one).
 * Actors are found by their spawn list key, like ActorSync. Not queued: they only matter while in the room.
 */
enum WorldEventKind : u8 {
    WORLD_EVENT_TORCH_LIT,
    WORLD_EVENT_SILVER_RUPEE,
};

static s16 sEventScene = -1;
static std::unordered_set<uint64_t> sTorchesToLight;
static std::unordered_set<uint64_t> sSilverRupeesToCollect;
// State before the actor's update, to see what the local player did in it
static std::unordered_map<Actor*, s16> sTorchLitTimer;
static std::unordered_map<Actor*, void*> sSilverAction;
// Changed by a teammate's event this frame, so not sent back
static std::unordered_set<Actor*> sForced;

static void ResetWorldEvents() {
    sEventScene = gPlayState != nullptr ? gPlayState->sceneNum : -1;
    sTorchesToLight.clear();
    sSilverRupeesToCollect.clear();
    sTorchLitTimer.clear();
    sSilverAction.clear();
    sForced.clear();
}

static bool SilverRupeesShuffled() {
    // Shuffled silver rupees are items with their own flags
    return IS_RANDO && RAND_GET_OPTION(RSK_SHUFFLE_SILVER);
}

} // namespace AnchorWorldSync

using namespace AnchorWorldSync;

// The team state on the server is replaced (and its queue emptied) by every UPDATE_TEAM_STATE, so we send ours now
// and then while playing. That keeps the queue short, so it never drops anything
constexpr auto TEAM_STATE_PUSH_INTERVAL = std::chrono::seconds(60);
// Frames to wait after a merge before pushing, so the queued packets that came with the state are applied first
constexpr int TEAM_STATE_PUSH_DELAY = 20;

void Anchor::SendPacket_WorldEvent(u8 kind, uint64_t actorKey) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = WORLD_EVENT;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["sceneNum"] = gPlayState->sceneNum;
    payload["kind"] = kind;
    payload["key"] = actorKey;
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_WorldEvent(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags ||
        payload.value("sceneNum", (s16)-1) != gPlayState->sceneNum) {
        return;
    }
    if (sEventScene != gPlayState->sceneNum) {
        ResetWorldEvents();
    }

    uint64_t key = payload.at("key").get<uint64_t>();
    switch (payload.at("kind").get<u8>()) {
        case WORLD_EVENT_TORCH_LIT:
            sTorchesToLight.insert(key);
            break;
        case WORLD_EVENT_SILVER_RUPEE:
            sSilverRupeesToCollect.insert(key);
            break;
    }
}

void Anchor::RegisterWorldSyncHooks() {
    COND_HOOK(OnSceneSpawnActors, isConnected, [&]() {
        RefreshSnapshot();
        ResetWorldEvents();

        static auto lastPush = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (hasMergedTeamState && now - lastPush > TEAM_STATE_PUSH_INTERVAL) {
            lastPush = now;
            SendPacket_UpdateTeamState();
        }
    });

    COND_HOOK(OnPlayerUpdate, isConnected, [&]() {
        if (TakeBeanPlanted()) {
            SendPacket_UpdateBeansCount();
        }

        static int pushDelay = 0;
        if (pendingTeamStatePush && ++pushDelay >= TEAM_STATE_PUSH_DELAY) {
            pushDelay = 0;
            pendingTeamStatePush = false;
            SendPacket_UpdateTeamState();
            SendPacket_MergeTeamState();
        }
    });

    // Floor and eye switches only look at their flag when they spawn, so press (or release) them when a teammate
    // flips it. Hold switches (subtypes 2 and 3) follow whoever stands on them, so they're left alone
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_OBJ_SWITCH, isConnected, [&](void* refActor, bool* should) {
        ObjSwitch* actor = static_cast<ObjSwitch*>(refActor);
        s32 type = actor->dyna.actor.params & 7;
        s32 subType = (actor->dyna.actor.params >> 4) & 7;
        bool on = Flags_GetSwitch(gPlayState, (actor->dyna.actor.params >> 8) & 0x3F);

        if (type == OBJSWITCH_TYPE_FLOOR || type == OBJSWITCH_TYPE_FLOOR_RUSTY) {
            bool isToggle = type == OBJSWITCH_TYPE_FLOOR && subType == OBJSWITCH_SUBTYPE_FLOOR_1;
            bool staysDown = type == OBJSWITCH_TYPE_FLOOR_RUSTY || subType == OBJSWITCH_SUBTYPE_FLOOR_0;
            if (actor->actionFunc == ObjSwitch_FloorUp && on && (staysDown || isToggle)) {
                actor->cooldownOn = false;
                ObjSwitch_FloorPressInit(actor);
            } else if (actor->actionFunc == ObjSwitch_FloorDown && !on && isToggle) {
                actor->cooldownOn = false;
                ObjSwitch_FloorReleaseInit(actor);
            }
        } else if (type == OBJSWITCH_TYPE_EYE) {
            if (actor->actionFunc == ObjSwitch_EyeOpen && on) {
                actor->cooldownOn = false;
                ObjSwitch_EyeClosingInit(actor);
            } else if (actor->actionFunc == ObjSwitch_EyeClosed && !on && subType == OBJSWITCH_SUBTYPE_EYE_1) {
                actor->cooldownOn = false;
                ObjSwitch_EyeOpeningInit(actor);
            }
        }
    });

    // Dodongo's Cavern stairs
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_BG_DDAN_KD, isConnected, [&](void* refActor, bool* should) {
        BgDdanKd* actor = static_cast<BgDdanKd*>(refActor);

        if (actor->actionFunc == BgDdanKd_CheckForExplosions && Flags_GetSwitch(gPlayState, actor->dyna.actor.params)) {
            actor->actionFunc = BgDdanKd_LowerStairs;
        }
    });

    // Golden Gauntlets pillar a teammate threw
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_BG_HEAVY_BLOCK, isConnected, [&](void* refActor, bool* should) {
        BgHeavyBlock* actor = static_cast<BgHeavyBlock*>(refActor);

        if ((actor->dyna.actor.params & 0xFF) == HEAVYBLOCK_BREAKABLE && actor->actionFunc == BgHeavyBlock_Wait &&
            Flags_GetSwitch(gPlayState, (actor->dyna.actor.params >> 8) & 0x3F)) {
            BgHeavyBlock_SpawnPieces(actor, gPlayState);
            SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->dyna.actor.world.pos, 40, NA_SE_EV_WALL_BROKEN);
            Actor_Kill(&actor->dyna.actor);
            *should = false;
        }
    });

    // Spirit Temple face a teammate shone light on
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_BG_JYA_MEGAMI, isConnected, [&](void* refActor, bool* should) {
        BgJyaMegami* actor = static_cast<BgJyaMegami*>(refActor);

        if (actor->actionFunc == BgJyaMegami_DetectLight &&
            Flags_GetSwitch(gPlayState, actor->dyna.actor.params & 0x3F)) {
            actor->lightTimer = 41;
        }
    });

    // Soft soil a teammate planted a bean in
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_OBJ_BEAN, isConnected, [&](void* refActor, bool* should) {
        ObjBean* actor = static_cast<ObjBean*>(refActor);

        if (actor->actionFunc == ObjBean_WaitForBean && Flags_GetSwitch(gPlayState, actor->dyna.actor.params & 0x3F)) {
            func_80B8FE6C(actor);
        }
    });

    // Torches: one of a group lit by a teammate, lit here as if by a fire arrow so this group counts it too
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_OBJ_SYOKUDAI, isConnected, [&](void* refActor, bool* should) {
        ObjSyokudai* actor = static_cast<ObjSyokudai*>(refActor);
        uint64_t key;

        if (!sTorchesToLight.empty() && AnchorActorSync::GetActorKey(&actor->actor, &key) &&
            sTorchesToLight.erase(key) && actor->litTimer == 0) {
            static ColliderInfo sFireArrow = {};
            sFireArrow.toucher.dmgFlags = 0x800;
            actor->colliderFlame.base.acFlags |= AC_HIT;
            actor->colliderFlame.info.acHitInfo = &sFireArrow;
            sForced.insert(&actor->actor);
        }
        sTorchLitTimer[&actor->actor] = actor->litTimer;
    });

    COND_ID_HOOK(OnActorUpdate, ACTOR_OBJ_SYOKUDAI, isConnected, [&](void* refActor) {
        ObjSyokudai* actor = static_cast<ObjSyokudai*>(refActor);
        uint64_t key;

        auto before = sTorchLitTimer.find(&actor->actor);
        bool forced = sForced.erase(&actor->actor) > 0;
        // Timed lighting only: a finished group sets its flag, which syncs by itself
        if (before != sTorchLitTimer.end() && before->second == 0 && actor->litTimer > 0 && !forced &&
            AnchorActorSync::GetActorKey(&actor->actor, &key)) {
            SendPacket_WorldEvent(WORLD_EVENT_TORCH_LIT, key);
        }
    });

    // Silver rupees: each one collected counts for everyone in the room
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_EN_G_SWITCH, isConnected, [&](void* refActor, bool* should) {
        EnGSwitch* actor = static_cast<EnGSwitch*>(refActor);
        if (actor->type == ENGSWITCH_SILVER_RUPEE) {
            sSilverAction[&actor->actor] = (void*)actor->actionFunc;
        }
    });

    COND_VB_SHOULD(VB_SILVER_COLLECT, isConnected, {
        EnGSwitch* actor = va_arg(args, EnGSwitch*);
        uint64_t key;

        if (!*should && !sSilverRupeesToCollect.empty() && !SilverRupeesShuffled() &&
            AnchorActorSync::GetActorKey(&actor->actor, &key) && sSilverRupeesToCollect.erase(key)) {
            *should = true;
            sForced.insert(&actor->actor);
        }
    });

    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_G_SWITCH, isConnected, [&](void* refActor) {
        EnGSwitch* actor = static_cast<EnGSwitch*>(refActor);
        uint64_t key;

        if (actor->type != ENGSWITCH_SILVER_RUPEE) {
            return;
        }
        auto before = sSilverAction.find(&actor->actor);
        bool forced = sForced.erase(&actor->actor) > 0;
        if (before != sSilverAction.end() && before->second == (void*)EnGSwitch_SilverRupeeIdle &&
            actor->actionFunc == EnGSwitch_SilverRupeeCollected && !forced && !SilverRupeesShuffled() &&
            AnchorActorSync::GetActorKey(&actor->actor, &key)) {
            SendPacket_WorldEvent(WORLD_EVENT_SILVER_RUPEE, key);
        }
    });

    COND_HOOK(OnActorDestroy, isConnected, [&](void* refActor) {
        Actor* actor = static_cast<Actor*>(refActor);
        if (actor->id == ACTOR_OBJ_SYOKUDAI || actor->id == ACTOR_EN_G_SWITCH) {
            sTorchLitTimer.erase(actor);
            sSilverAction.erase(actor);
            sForced.erase(actor);
        }
    });
}
