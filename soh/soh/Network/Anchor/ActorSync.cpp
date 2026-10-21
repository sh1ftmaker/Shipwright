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
#include "src/overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
#include "src/overlays/actors/ovl_Boss_Ganondrof/z_boss_ganondrof.h"
#include "src/overlays/actors/ovl_Boss_Fd/z_boss_fd.h"
#include "src/overlays/actors/ovl_Boss_Fd2/z_boss_fd2.h"
#include "src/overlays/actors/ovl_Boss_Sst/z_boss_sst.h"
#include "src/overlays/actors/ovl_En_fHG/z_en_fhg.h"
#include "src/overlays/actors/ovl_En_Ik/z_en_ik.h"
#include "src/overlays/actors/ovl_En_Test/z_en_test.h"
#include "src/overlays/actors/ovl_En_Zf/z_en_zf.h"
#include "src/overlays/actors/ovl_En_Dh/z_en_dh.h"
#include "src/overlays/actors/ovl_En_Vali/z_en_vali.h"
#include "src/overlays/actors/ovl_En_Wf/z_en_wf.h"
#include "src/overlays/actors/ovl_En_Rd/z_en_rd.h"
#include "src/overlays/actors/ovl_En_Fz/z_en_fz.h"
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
void BossDodongo_Walk(BossDodongo* kd, PlayState* play);
void BossDodongo_Inhale(BossDodongo* kd, PlayState* play);
void BossDodongo_BlowFire(BossDodongo* kd, PlayState* play);
void BossDodongo_Roll(BossDodongo* kd, PlayState* play);
void BossDodongo_Explode(BossDodongo* kd, PlayState* play);
void BossDodongo_LayDown(BossDodongo* kd, PlayState* play);
void BossDodongo_Vulnerable(BossDodongo* kd, PlayState* play);
void BossDodongo_GetUp(BossDodongo* kd, PlayState* play);
void BossDodongo_Damaged(BossDodongo* kd, PlayState* play);
void BossDodongo_DeathCutscene(BossDodongo* kd, PlayState* play);
void BossDodongo_SetupDeathCutscene(BossDodongo* kd);
void BossDodongo_SetupExplode(BossDodongo* kd);
void BossGanondrof_Paintings(BossGanondrof* pg, PlayState* play);
void BossGanondrof_Neutral(BossGanondrof* pg, PlayState* play);
void BossGanondrof_Throw(BossGanondrof* pg, PlayState* play);
void BossGanondrof_Block(BossGanondrof* pg, PlayState* play);
void BossGanondrof_Return(BossGanondrof* pg, PlayState* play);
void BossGanondrof_Charge(BossGanondrof* pg, PlayState* play);
void BossGanondrof_Stunned(BossGanondrof* pg, PlayState* play);
void BossGanondrof_Death(BossGanondrof* pg, PlayState* play);
void BossGanondrof_SetupDeath(BossGanondrof* pg, PlayState* play);
void BossFd_Fly(BossFd* fd, PlayState* play);
void BossFd_Wait(BossFd* fd, PlayState* play);
void BossFd2_Emerge(BossFd2* fd2, PlayState* play);
void BossFd2_Idle(BossFd2* fd2, PlayState* play);
void BossFd2_Burrow(BossFd2* fd2, PlayState* play);
void BossFd2_BreatheFire(BossFd2* fd2, PlayState* play);
void BossFd2_ClawSwipe(BossFd2* fd2, PlayState* play);
void BossFd2_Vulnerable(BossFd2* fd2, PlayState* play);
void BossFd2_Damaged(BossFd2* fd2, PlayState* play);
void BossFd2_Death(BossFd2* fd2, PlayState* play);
void BossFd2_Wait(BossFd2* fd2, PlayState* play);
void BossFd2_SetupDeath(BossFd2* fd2, PlayState* play);
void BossSst_HeadNeutral(BossSst* sst, PlayState* play);
void BossSst_HeadWait(BossSst* sst, PlayState* play);
void BossSst_HeadDamagedHand(BossSst* sst, PlayState* play);
void BossSst_HeadReadyCharge(BossSst* sst, PlayState* play);
void BossSst_HeadCharge(BossSst* sst, PlayState* play);
void BossSst_HeadEndCharge(BossSst* sst, PlayState* play);
void BossSst_HeadFrozenHand(BossSst* sst, PlayState* play);
void BossSst_HeadUnfreezeHand(BossSst* sst, PlayState* play);
void BossSst_HeadStunned(BossSst* sst, PlayState* play);
void BossSst_HeadVulnerable(BossSst* sst, PlayState* play);
void BossSst_HeadDamage(BossSst* sst, PlayState* play);
void BossSst_HeadRecover(BossSst* sst, PlayState* play);
void BossSst_HeadDeath(BossSst* sst, PlayState* play);
void BossSst_HeadThrash(BossSst* sst, PlayState* play);
void BossSst_HeadDarken(BossSst* sst, PlayState* play);
void BossSst_HeadFall(BossSst* sst, PlayState* play);
void BossSst_HeadMelt(BossSst* sst, PlayState* play);
void BossSst_HeadFinish(BossSst* sst, PlayState* play);
void BossSst_HandWait(BossSst* sst, PlayState* play);
void BossSst_HandDownbeat(BossSst* sst, PlayState* play);
void BossSst_HandDownbeatEnd(BossSst* sst, PlayState* play);
void BossSst_HandOffbeat(BossSst* sst, PlayState* play);
void BossSst_HandOffbeatEnd(BossSst* sst, PlayState* play);
void BossSst_HandReadySlam(BossSst* sst, PlayState* play);
void BossSst_HandSlam(BossSst* sst, PlayState* play);
void BossSst_HandEndSlam(BossSst* sst, PlayState* play);
void BossSst_HandReadySweep(BossSst* sst, PlayState* play);
void BossSst_HandSweep(BossSst* sst, PlayState* play);
void BossSst_HandReadyPunch(BossSst* sst, PlayState* play);
void BossSst_HandPunch(BossSst* sst, PlayState* play);
void BossSst_HandReadyClap(BossSst* sst, PlayState* play);
void BossSst_HandClap(BossSst* sst, PlayState* play);
void BossSst_HandEndClap(BossSst* sst, PlayState* play);
void BossSst_HandReadyGrab(BossSst* sst, PlayState* play);
void BossSst_HandGrab(BossSst* sst, PlayState* play);
void BossSst_HandCrush(BossSst* sst, PlayState* play);
void BossSst_HandEndCrush(BossSst* sst, PlayState* play);
void BossSst_HandSwing(BossSst* sst, PlayState* play);
void BossSst_HandRetreat(BossSst* sst, PlayState* play);
void BossSst_HandReel(BossSst* sst, PlayState* play);
void BossSst_HandReadyShake(BossSst* sst, PlayState* play);
void BossSst_HandShake(BossSst* sst, PlayState* play);
void BossSst_HandReadyCharge(BossSst* sst, PlayState* play);
void BossSst_HandFrozen(BossSst* sst, PlayState* play);
void BossSst_HandReadyBreakIce(BossSst* sst, PlayState* play);
void BossSst_HandBreakIce(BossSst* sst, PlayState* play);
void BossSst_HandStunned(BossSst* sst, PlayState* play);
void BossSst_HandDamage(BossSst* sst, PlayState* play);
void BossSst_HandRecover(BossSst* sst, PlayState* play);
void BossSst_HandThrash(BossSst* sst, PlayState* play);
void BossSst_HandDarken(BossSst* sst, PlayState* play);
void BossSst_HandFall(BossSst* sst, PlayState* play);
void BossSst_HandMelt(BossSst* sst, PlayState* play);
void BossSst_HandFinish(BossSst* sst, PlayState* play);
void BossSst_HeadSetupDeath(BossSst* head, PlayState* play);
void BossSst_HandSetupDamage(BossSst* hand);
void BossSst_HandReleasePlayer(BossSst* hand, PlayState* play, s32 dropPlayer);
s32* BossSst_GetHandStates(void);
void EnfHG_Approach(EnfHG* horse, PlayState* play);
void EnfHG_Attack(EnfHG* horse, PlayState* play);
void EnfHG_Damage(EnfHG* horse, PlayState* play);
void EnfHG_Retreat(EnfHG* horse, PlayState* play);
void EnfHG_Done(EnfHG* horse, PlayState* play);
void func_80A747C0(EnIk* ik, PlayState* play); // Iron Knuckle: dormant until hit or approached
void func_80A7492C(EnIk* ik, PlayState* play); // Idle
void func_80A74BA4(EnIk* ik, PlayState* play); // Walk
void func_80A74EBC(EnIk* ik, PlayState* play); // Vertical swing
void func_80A7510C(EnIk* ik, PlayState* play); // Pull the axe out of the floor
void func_80A75260(EnIk* ik, PlayState* play); // Horizontal swing
void func_80A7545C(EnIk* ik, PlayState* play); // Recover
void func_80A75530(EnIk* ik, PlayState* play); // Turn-around swing
void func_80A7567C(EnIk* ik, PlayState* play); // Block
void func_80A758B0(EnIk* ik, PlayState* play); // Knocked back
void func_80A75A38(EnIk* ik, PlayState* play); // Death
void func_80A7598C(EnIk* ik);                  // Setup death
void func_80A75FA0(Actor* thisx, PlayState* play); // The fight's update (the Nabooru cutscenes use another)
void EnTest_Idle(EnTest* stalfos, PlayState* play);
void EnTest_WalkAndBlock(EnTest* stalfos, PlayState* play);
void func_80860C24(EnTest* stalfos, PlayState* play); // Walk around Link
void func_80860F84(EnTest* stalfos, PlayState* play); // Sidestep
void EnTest_SlashDown(EnTest* stalfos, PlayState* play);
void EnTest_SlashDownEnd(EnTest* stalfos, PlayState* play);
void EnTest_SlashUp(EnTest* stalfos, PlayState* play);
void EnTest_JumpBack(EnTest* stalfos, PlayState* play);
void EnTest_Jumpslash(EnTest* stalfos, PlayState* play);
void EnTest_JumpUp(EnTest* stalfos, PlayState* play);
void EnTest_StopAndBlock(EnTest* stalfos, PlayState* play);
void EnTest_IdleFromBlock(EnTest* stalfos, PlayState* play);
void func_808621D4(EnTest* stalfos, PlayState* play); // Hit from the front
void func_80862418(EnTest* stalfos, PlayState* play); // Hit from behind
void EnTest_Stunned(EnTest* stalfos, PlayState* play);
void func_808628C8(EnTest* stalfos, PlayState* play); // Sidestep
void func_80862E6C(EnTest* stalfos, PlayState* play); // Collapsed, until it gets up again or its partner falls too
void EnTest_Recoil(EnTest* stalfos, PlayState* play);
void func_80863044(EnTest* stalfos, PlayState* play); // Falling over backwards
void func_8086318C(EnTest* stalfos, PlayState* play); // Falling over forwards
void func_808633E8(EnTest* stalfos, PlayState* play); // Breaking apart
void func_80862FA8(EnTest* stalfos, PlayState* play); // Setup falling over
void func_80B4543C(EnZf* zf, PlayState* play); // Idle
void EnZf_ApproachPlayer(EnZf* zf, PlayState* play);
void EnZf_JumpForward(EnZf* zf, PlayState* play);
void func_80B46098(EnZf* zf, PlayState* play); // Strafe
void func_80B462E4(EnZf* zf, PlayState* play);
void func_80B463E4(EnZf* zf, PlayState* play); // Circle
void EnZf_Slash(EnZf* zf, PlayState* play);
void EnZf_RecoilFromBlockedSlash(EnZf* zf, PlayState* play);
void EnZf_JumpBack(EnZf* zf, PlayState* play);
void EnZf_Stunned(EnZf* zf, PlayState* play);
void EnZf_SheatheSword(EnZf* zf, PlayState* play);
void EnZf_HopAndTaunt(EnZf* zf, PlayState* play);
void EnZf_HopAway(EnZf* zf, PlayState* play);
void EnZf_DrawSword(EnZf* zf, PlayState* play);
void EnZf_Damaged(EnZf* zf, PlayState* play);
void EnZf_JumpUp(EnZf* zf, PlayState* play);
void func_80B483E4(EnZf* zf, PlayState* play); // Block
void EnZf_CircleAroundPlayer(EnZf* zf, PlayState* play);
void EnZf_Die(EnZf* zf, PlayState* play);
void EnZf_SetupDie(EnZf* zf);
s16* EnZf_GetPairState(s32 which);
void EnDh_Wait(EnDh* dh, PlayState* play);
void EnDh_Walk(EnDh* dh, PlayState* play);
void EnDh_Retreat(EnDh* dh, PlayState* play);
void EnDh_Attack(EnDh* dh, PlayState* play);
void EnDh_Burrow(EnDh* dh, PlayState* play);
void EnDh_Damage(EnDh* dh, PlayState* play);
void EnDh_Death(EnDh* dh, PlayState* play);
void EnDh_SetupDeath(EnDh* dh);
void EnVali_Lurk(EnVali* bari, PlayState* play);
void EnVali_DropAppear(EnVali* bari, PlayState* play);
void EnVali_FloatIdle(EnVali* bari, PlayState* play);
void EnVali_Attacked(EnVali* bari, PlayState* play);
void EnVali_Retaliate(EnVali* bari, PlayState* play);
void EnVali_MoveArmsDown(EnVali* bari, PlayState* play);
void EnVali_Burnt(EnVali* bari, PlayState* play);
void EnVali_DivideAndDie(EnVali* bari, PlayState* play);
void EnVali_Stunned(EnVali* bari, PlayState* play);
void EnVali_Frozen(EnVali* bari, PlayState* play);
void EnVali_ReturnToLurk(EnVali* bari, PlayState* play);
void EnVali_SetupDivideAndDie(EnVali* bari, PlayState* play);
void EnVali_Draw(Actor* thisx, PlayState* play);
void EnWf_Wait(EnWf* wolfos, PlayState* play);
void EnWf_RunAtPlayer(EnWf* wolfos, PlayState* play);
void EnWf_SearchForPlayer(EnWf* wolfos, PlayState* play);
void EnWf_RunAroundPlayer(EnWf* wolfos, PlayState* play);
void EnWf_Slash(EnWf* wolfos, PlayState* play);
void EnWf_RecoilFromBlockedSlash(EnWf* wolfos, PlayState* play);
void EnWf_BackflipAway(EnWf* wolfos, PlayState* play);
void EnWf_Stunned(EnWf* wolfos, PlayState* play);
void EnWf_Damaged(EnWf* wolfos, PlayState* play);
void EnWf_SomersaultAndAttack(EnWf* wolfos, PlayState* play);
void EnWf_Blocking(EnWf* wolfos, PlayState* play);
void EnWf_Sidestep(EnWf* wolfos, PlayState* play);
void EnWf_Die(EnWf* wolfos, PlayState* play);
void EnWf_SetupDie(EnWf* wolfos);
void EnRd_Idle(EnRd* redead, PlayState* play);
void EnRd_WalkToPlayer(EnRd* redead, PlayState* play);
void EnRd_WalkToHome(EnRd* redead, PlayState* play);
void EnRd_WalkToParent(EnRd* redead, PlayState* play);
void EnRd_Grab(EnRd* redead, PlayState* play);
void EnRd_AttemptPlayerFreeze(EnRd* redead, PlayState* play);
void EnRd_StandUp(EnRd* redead, PlayState* play);
void EnRd_Crouch(EnRd* redead, PlayState* play);
void EnRd_Damaged(EnRd* redead, PlayState* play);
void EnRd_Stunned(EnRd* redead, PlayState* play);
void EnRd_Dead(EnRd* redead, PlayState* play);
void EnRd_SetupDead(EnRd* redead);
void EnFz_Wait(EnFz* freezard, PlayState* play);
void EnFz_Appear(EnFz* freezard, PlayState* play);
void EnFz_AimForMove(EnFz* freezard, PlayState* play);
void EnFz_MoveTowardsPlayer(EnFz* freezard, PlayState* play);
void EnFz_AimForFreeze(EnFz* freezard, PlayState* play);
void EnFz_BlowSmoke(EnFz* freezard, PlayState* play);
void EnFz_Disappear(EnFz* freezard, PlayState* play);
void EnFz_BlowSmokeStationary(EnFz* freezard, PlayState* play);
void EnFz_Melt(EnFz* freezard, PlayState* play);
void EnFz_Despawn(EnFz* freezard, PlayState* play);
void EnFz_SetupDespawn(EnFz* freezard, PlayState* play);
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
s8 sRoom = -1;
uint32_t sRoomSince = 0; // The frame this player walked into the room it's in
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

constexpr uint32_t ROOM_TIME_SLACK = 60; // Players who walked in this close together count as arriving at once

// The player who has been in a room the longest runs its enemies, so someone who walks in (or comes back after
// leaving or reloading) follows the fight that's going on instead of restarting it from their fresh copy. When that
// player leaves, whoever has been there longest after them carries on. Players who arrived together (a warp) go by
// lowest client ID. When nobody is in it, everyone runs their own.
uint32_t RoomOwner(s8 room) {
    Anchor* anchor = Anchor::Instance;
    std::vector<std::pair<uint32_t, uint32_t>> present; // Client ID, frames in the room
    if (gPlayState->roomCtx.curRoom.num == room) {
        present.emplace_back(anchor->ownClientId, sFrame - sRoomSince);
    }
    for (auto& [clientId, client] : anchor->clients) {
        if (IsPeer(client) && client.curRoomNum == room) {
            present.emplace_back(clientId, client.roomFrames);
        }
    }
    uint32_t longest = 0;
    for (auto& [clientId, frames] : present) {
        longest = std::max(longest, frames);
    }
    uint32_t owner = UINT32_MAX;
    for (auto& [clientId, frames] : present) {
        if (frames + ROOM_TIME_SLACK >= longest && clientId < owner) {
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
        case ACTOR_EN_DHA: // Dead Hand's hands: each game's grab its own Link
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

// A file static of the actor's overlay that the fight shares, through an accessor the overlay exports
struct SyncStatic {
    void* (*get)();
    u16 size;
};

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
    bool (*match)(Actor* actor) = nullptr;        // Which actors with this ID (parts can share it); all if not set
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
    std::vector<SyncStatic> statics = {};
    void (*onApply)(Actor* actor, const void* before) = nullptr; // After taking someone else's state (start effects)
    bool (*defeat)(Actor* actor) = nullptr; // Starts the death on a game that didn't land the blow (false: not yet)
    std::vector<GroupMember> members = {};        // Other actors in the same fight, sent along with this one
    bool isMember = false;                        // Only synced as part of another actor's group
    bool targetsOwnLink = false;                  // Skip TargetNearestLink
};

// Each actor as its def describes it, plus a copy of its struct from before the frame's updates
struct Synced {
    const StateSyncDef* def = nullptr;
    std::vector<Actor*> actors;          // The leader, then its members (nullptr when not there)
    std::vector<std::vector<u8>> before; // Each as it was at the last check
    uint32_t sentSeq = 0;                // The last state this game sent as the room owner
    uint32_t seq = 0;                    // The last one it took from the room owner...
    uint32_t seqFrom = 0;                // ...and who that was
    uint32_t hitSeq = 0;                 // The last hit this game sent
    uint32_t holdUntil = 0;              // Until then, the room owner's states are ignored unless they include that hit
    std::unordered_map<uint32_t, uint32_t> hitsTaken; // Room owner: the last hit from each player it has taken
    bool defeated = false;               // Seen or heard about, so it's sent once
    bool pendingDefeat = false;          // Defeated elsewhere while this game was still in the intro
    uint32_t applied = 0;                // For tests: member states applied, and why the last one wasn't
    const char* skipped = "";
    std::unordered_map<std::string, uint32_t> dropped; // For tests: room owner states not applied, by reason
};

constexpr uint32_t SYNC_HIT_HOLD = 40;
constexpr uint32_t SYNC_SEQ_RESTART = 60;

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
    for (const SyncStatic& field : def.statics) {
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
    for (const SyncStatic& field : def.statics) {
        put(field.get(), field.size);
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

    for (const SyncStatic& field : def.statics) {
        get(field.get(), field.size);
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
        synced.skipped = "member count";
        return;
    }
    for (size_t i = 0; i < synced.actors.size(); i++) {
        Actor* actor = synced.actors[i];
        const StateSyncDef& def = MemberDef(synced, i);
        if (actor == nullptr || actor->update == nullptr || !members[i].is_array()) {
            synced.skipped = actor == nullptr ? "no actor" : "no state";
            continue;
        }
        int action = members[i][0].get<int>();
        std::vector<u8> data = Unbase64(members[i][1].get<std::string>());
        if (data.size() != SnapshotSize(def) ||
            (def.actionOffset != NO_ACTION && (action < 0 || action >= (int)def.fightActions.size()))) {
            synced.skipped = "bad state";
            continue;
        }
        // Each game grabs its own Link
        if (IsIn(def.localActions, ActionOf(actor, def)) ||
            (def.actionOffset != NO_ACTION && IsIn(def.localActions, def.fightActions[action]))) {
            synced.skipped = "local action";
            continue;
        }
        synced.applied++;
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
    payload["seq"] = ++synced.sentSeq;
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
    synced.pendingDefeat = synced.def->defeat != nullptr && !synced.def->defeat(leader);
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
        return true;
    },
};

const StateSyncDef sKingDodongo = {
    .name = "King Dodongo",
    .actorId = ACTOR_BOSS_DODONGO,
    .size = sizeof(BossDodongo),
    .actionOffset = offsetof(BossDodongo, actionFunc),
    .fightActions = { ACTION(BossDodongo_Walk), ACTION(BossDodongo_Inhale), ACTION(BossDodongo_BlowFire),
                      ACTION(BossDodongo_Roll), ACTION(BossDodongo_Explode), ACTION(BossDodongo_LayDown),
                      ACTION(BossDodongo_Vulnerable), ACTION(BossDodongo_GetUp), ACTION(BossDodongo_Damaged) },
    // Explode: he swallowed a bomb, which only the game that threw it sees
    .hitReactions = { ACTION(BossDodongo_Explode), ACTION(BossDodongo_Damaged) },
    .deathActions = { ACTION(BossDodongo_DeathCutscene) },
    // A bomb or explosion in his mouth while he breathes in makes him give up and walk on
    .wasHit = [](Actor* actor, const void* before) {
        return ((const BossDodongo*)before)->actionFunc == BossDodongo_Inhale &&
               ((BossDodongo*)actor)->actionFunc == BossDodongo_Walk && ((BossDodongo*)actor)->unk_1DA == 50;
    },
    .skelAnimes = { offsetof(BossDodongo, skelAnime) },
    .health = { offsetof(BossDodongo, health), 2 },
    // Not the cameras, the lava textures or what's worked out from the local Link each frame
    .fields = { FIELDS(BossDodongo, unk_19E, unk_1A2), FIELDS(BossDodongo, numWallCollisions, unk_1B0),
                FIELDS(BossDodongo, unk_1BE, unk_1C0), FIELD(BossDodongo, unk_1C4), FIELD(BossDodongo, unk_1C8),
                FIELDS(BossDodongo, unk_1DA, unk_1DE), FIELDS(BossDodongo, unk_1E4, unk_1EC),
                FIELDS(BossDodongo, unk_1F8, unk_200), FIELD(BossDodongo, unk_208), FIELD(BossDodongo, unk_228),
                FIELDS(BossDodongo, unk_240, unk_244), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    .defeat = [](Actor* actor) {
        // His death cutscene fires the defeat hook
        ((BossDodongo*)actor)->health = 0;
        BossDodongo_SetupDeathCutscene((BossDodongo*)actor);
        Enemy_StartFinishingBlow(gPlayState, actor);
        return true;
    },
};

// Phantom Ganon's horse, which carries him through the paintings. The decoy that rides out of another painting is
// each game's own. Not registered yet (see StateSyncDefs).
const StateSyncDef sPhantomGanonHorse = {
    .name = "Phantom Ganon's horse",
    .actorId = ACTOR_EN_FHG,
    .size = sizeof(EnfHG),
    .match = [](Actor* actor) { return actor->params < GND_FAKE_BOSS; },
    .actionOffset = offsetof(EnfHG, actionFunc),
    .fightActions = { ACTION(EnfHG_Approach), ACTION(EnfHG_Attack), ACTION(EnfHG_Damage), ACTION(EnfHG_Retreat),
                      ACTION(EnfHG_Done) },
    .hitReactions = { ACTION(EnfHG_Damage) },
    .skelAnimes = { offsetof(EnfHG, skin.skelAnime) },
    // hitTimer: a hit on Phantom Ganon in a painting tells the horse to turn back
    .fields = { FIELD(EnfHG, bossGndInPainting), FIELD(EnfHG, fhgFireKillWarp),
                FIELDS(EnfHG, inPaintingPos, approachRate), FIELDS(EnfHG, gallopTimer, spawnedWarp),
                FIELDS(EnfHG, timers, turnRot), FIELDS(EnfHG, warpColorFilterR, warpColorFilterUnk2),
                FIELD(Actor, scale) },
    .isMember = true,
};

const StateSyncDef sPhantomGanon = {
    .name = "Phantom Ganon",
    .actorId = ACTOR_BOSS_GANONDROF,
    .size = sizeof(BossGanondrof),
    .match = [](Actor* actor) { return actor->params < GND_FAKE_BOSS; },
    .actionOffset = offsetof(BossGanondrof, actionFunc),
    .fightActions = { ACTION(BossGanondrof_Paintings), ACTION(BossGanondrof_Neutral), ACTION(BossGanondrof_Throw),
                      ACTION(BossGanondrof_Block), ACTION(BossGanondrof_Return), ACTION(BossGanondrof_Charge),
                      ACTION(BossGanondrof_Stunned) },
    .hitReactions = { ACTION(BossGanondrof_Stunned) },
    .deathActions = { ACTION(BossGanondrof_Death) },
    .skelAnimes = { offsetof(BossGanondrof, skelAnime) },
    // Each game rallies its own energy balls, at its own Link, so not the return count they leave
    .fields = { FIELD(BossGanondrof, work), FIELD(BossGanondrof, timers), FIELD(BossGanondrof, shockTimer),
                FIELD(BossGanondrof, flyMode), FIELD(BossGanondrof, fwork), FIELD(Actor, scale),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    // Out of the paintings for good: what his own change to flying does besides the action
    .onApply = [](Actor* actor, const void* before) {
        BossGanondrof* pg = (BossGanondrof*)actor;
        if (pg->flyMode != GND_FLY_PAINTING && ((const BossGanondrof*)before)->flyMode == GND_FLY_PAINTING) {
            pg->colliderBody.dim.radius = 20;
            pg->colliderBody.dim.height = 60;
            pg->colliderBody.dim.yShift = -33;
            actor->naviEnemyId = 0x1A;
        }
    },
    .defeat = [](Actor* actor) {
        // His own hit handler fires the defeat hook, not his death
        actor->colChkInfo.health = 0;
        BossGanondrof_SetupDeath((BossGanondrof*)actor, gPlayState);
        Enemy_StartFinishingBlow(gPlayState, actor);
        GameInteractor_ExecuteOnBossDefeat(actor);
        return true;
    },
    .members = { { &sPhantomGanonHorse,
                   [](Actor* pg) {
                       return pg->child != nullptr && pg->child->id == ACTOR_EN_FHG ? pg->child : nullptr;
                   } } },
};

// Volvagia: the flying body (Boss_Fd) and the head that comes out of the holes (Boss_Fd2), which hand the fight to each
// other through the body's handoffSignal, so they're sent together. The health is the body's.
BossFd2* FindFd2(Actor* fd) {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != nullptr; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_FD2 && actor->parent == fd && actor->update != nullptr) {
            return (BossFd2*)actor;
        }
    }
    return nullptr;
}

const StateSyncDef sVolvagiaHead = {
    .name = "Volvagia's head",
    .actorId = ACTOR_BOSS_FD2,
    .size = sizeof(BossFd2),
    .actionOffset = offsetof(BossFd2, actionFunc),
    .fightActions = { ACTION(BossFd2_Wait), ACTION(BossFd2_Emerge), ACTION(BossFd2_Idle), ACTION(BossFd2_Burrow),
                      ACTION(BossFd2_BreatheFire), ACTION(BossFd2_ClawSwipe), ACTION(BossFd2_Vulnerable),
                      ACTION(BossFd2_Damaged) },
    .hitReactions = { ACTION(BossFd2_Vulnerable), ACTION(BossFd2_Damaged) },
    .deathActions = { ACTION(BossFd2_Death) },
    .skelAnimes = { offsetof(BossFd2, skelAnime) },
    .fields = { FIELD(BossFd2, work), FIELD(BossFd2, timers), FIELD(BossFd2, fwork), FIELD(BossFd2, eyeState),
                FIELD(BossFd2, jawOpening) },
    .isMember = true,
};

const StateSyncDef sVolvagia = {
    .name = "Volvagia",
    .actorId = ACTOR_BOSS_FD,
    .size = sizeof(BossFd),
    .actionOffset = offsetof(BossFd, actionFunc),
    .fightActions = { ACTION(BossFd_Fly), ACTION(BossFd_Wait) },
    // BossFd_Fly also plays the intro and the end of the death, by its state
    .isFighting = [](Actor* actor) {
        BossFd* fd = (BossFd*)actor;
        BossFd2* fd2 = FindFd2(actor);
        return fd->introState == BFD_CS_NONE && fd2 != nullptr && fd2->deathState == 0 &&
               fd2->actionFunc != BossFd2_Death && (fd->actionFunc == BossFd_Wait || fd->work[BFD_ACTION_STATE] < 200);
    },
    .isDefeated = [](Actor* actor) {
        BossFd2* fd2 = FindFd2(actor);
        return actor->colChkInfo.health == 0 && (fd2 == nullptr || fd2->actionFunc == BossFd2_Death);
    },
    // His flying head takes damage without changing what he does
    .wasHit = [](Actor* actor, const void* before) {
        return ((BossFd*)actor)->work[BFD_DAMAGE_FLASH_TIMER] > ((const BossFd*)before)->work[BFD_DAMAGE_FLASH_TIMER];
    },
    .skelAnimes = { offsetof(BossFd, skelAnimeHead), offsetof(BossFd, skelAnimeRightArm),
                    offsetof(BossFd, skelAnimeLeftArm) },
    // The body follows the head's path, so not the segments; not the intro, the cameras or the effects
    .fields = { FIELDS(BossFd, fireBreathTimer, skinSegments), FIELD(BossFd, fogMode), FIELD(BossFd, work),
                FIELD(BossFd, timers), FIELD(BossFd, fwork), FIELDS(BossFd, targetPosition, holePosition),
                FIELD(BossFd, holeIndex), FIELD(BossFd, eyeState), FIELDS(BossFd, faceExposed, handoffSignal),
                FIELDS(BossFd, flattenMane, jawOpening) },
    .defeat = [](Actor* actor) {
        // The head dies where it is, and the rest of the death (the body's fall, the warp) follows from it. While
        // he's flying, that waits for the head to come out.
        BossFd2* fd2 = FindFd2(actor);
        if (fd2 == nullptr || fd2->actionFunc == BossFd2_Wait) {
            return false;
        }
        actor->colChkInfo.health = 0;
        BossFd2_SetupDeath(fd2, gPlayState);
        fd2->work[FD2_DAMAGE_FLASH_TIMER] = 10;
        fd2->work[FD2_INVINC_TIMER] = 30000;
        Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x100FF);
        Enemy_StartFinishingBlow(gPlayState, &fd2->actor);
        // Boss defeat tracking is keyed on the head
        GameInteractor_ExecuteOnBossDefeat(&fd2->actor);
        return true;
    },
    .members = { { &sVolvagiaHead, [](Actor* fd) { return (Actor*)FindFd2(fd); } } },
};

// Bongo Bongo: the head and the two hands, which a hit always changes together, and the hands' shared states
Actor* FindBongoHand(s16 params) {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != nullptr; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_SST && actor->params == params && actor->update != nullptr) {
            return actor;
        }
    }
    return nullptr;
}

const StateSyncDef sBongoBongoHand = {
    .name = "Bongo Bongo's hand",
    .actorId = ACTOR_BOSS_SST,
    .size = sizeof(BossSst),
    .match = [](Actor* actor) { return actor->params != BONGO_HEAD; },
    .actionOffset = offsetof(BossSst, actionFunc),
    .fightActions = { ACTION(BossSst_HandWait), ACTION(BossSst_HandDownbeat), ACTION(BossSst_HandDownbeatEnd),
                      ACTION(BossSst_HandOffbeat), ACTION(BossSst_HandOffbeatEnd), ACTION(BossSst_HandReadySlam),
                      ACTION(BossSst_HandSlam), ACTION(BossSst_HandEndSlam), ACTION(BossSst_HandReadySweep),
                      ACTION(BossSst_HandSweep), ACTION(BossSst_HandReadyPunch), ACTION(BossSst_HandPunch),
                      ACTION(BossSst_HandReadyClap), ACTION(BossSst_HandClap), ACTION(BossSst_HandEndClap),
                      ACTION(BossSst_HandReadyGrab), ACTION(BossSst_HandGrab), ACTION(BossSst_HandCrush),
                      ACTION(BossSst_HandEndCrush), ACTION(BossSst_HandSwing), ACTION(BossSst_HandRetreat),
                      ACTION(BossSst_HandReel), ACTION(BossSst_HandReadyShake), ACTION(BossSst_HandShake),
                      ACTION(BossSst_HandReadyCharge), ACTION(BossSst_HandFrozen),
                      ACTION(BossSst_HandReadyBreakIce), ACTION(BossSst_HandBreakIce),
                      ACTION(BossSst_HandStunned), ACTION(BossSst_HandDamage), ACTION(BossSst_HandRecover) },
    .hitReactions = { ACTION(BossSst_HandReel), ACTION(BossSst_HandFrozen) },
    // Crushing or swinging this game's own Link
    .localActions = { ACTION(BossSst_HandCrush), ACTION(BossSst_HandSwing) },
    .deathActions = { ACTION(BossSst_HandThrash), ACTION(BossSst_HandDarken), ACTION(BossSst_HandFall),
                      ACTION(BossSst_HandMelt), ACTION(BossSst_HandFinish) },
    .skelAnimes = { offsetof(BossSst, skelAnime) },
    .fields = { FIELDS(BossSst, actionVar, targetRoll), FIELDS(BossSst, radius, center),
                FLAGS(BossSst, colliderJntSph.base.atFlags, AT_ON),
                FLAGS(BossSst, colliderJntSph.base.acFlags, AC_ON | AC_HARD),
                FIELD(BossSst, colliderJntSph.base.colType), FLAGS(BossSst, colliderCyl.base.atFlags, AT_ON),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE) },
    // A hand that let go on the room owner's game lets go of this Link too
    .onApply = [](Actor* actor, const void* before) {
        BossSst* hand = (BossSst*)actor;
        Player* player = GET_PLAYER(gPlayState);
        if (player->actor.parent == actor && hand->actionFunc != BossSst_HandGrab &&
            hand->actionFunc != BossSst_HandClap && hand->actionFunc != BossSst_HandCrush &&
            hand->actionFunc != BossSst_HandSwing) {
            BossSst_HandReleasePlayer(hand, gPlayState, true);
        }
    },
    .isMember = true,
};

const StateSyncDef sBongoBongo = {
    .name = "Bongo Bongo",
    .actorId = ACTOR_BOSS_SST,
    .size = sizeof(BossSst),
    .match = [](Actor* actor) { return actor->params == BONGO_HEAD; },
    .actionOffset = offsetof(BossSst, actionFunc),
    .fightActions = { ACTION(BossSst_HeadNeutral), ACTION(BossSst_HeadWait), ACTION(BossSst_HeadDamagedHand),
                      ACTION(BossSst_HeadReadyCharge), ACTION(BossSst_HeadCharge), ACTION(BossSst_HeadEndCharge),
                      ACTION(BossSst_HeadFrozenHand), ACTION(BossSst_HeadUnfreezeHand),
                      ACTION(BossSst_HeadStunned), ACTION(BossSst_HeadVulnerable), ACTION(BossSst_HeadDamage),
                      ACTION(BossSst_HeadRecover) },
    .hitReactions = { ACTION(BossSst_HeadStunned), ACTION(BossSst_HeadDamage), ACTION(BossSst_HeadDamagedHand) },
    .deathActions = { ACTION(BossSst_HeadDeath), ACTION(BossSst_HeadThrash), ACTION(BossSst_HeadDarken),
                      ACTION(BossSst_HeadFall), ACTION(BossSst_HeadMelt), ACTION(BossSst_HeadFinish) },
    .skelAnimes = { offsetof(BossSst, skelAnime) },
    // The eye can only be hit in some states, which their setups switch on
    .fields = { FIELDS(BossSst, actionVar, timer), FIELD(BossSst, targetYaw), FIELD(BossSst, radius),
                FLAGS(BossSst, colliderCyl.base.acFlags, AC_ON), FIELD(BossSst, colliderCyl.info.bumper.dmgFlags),
                FLAGS(BossSst, colliderItems[0].info.bumperFlags, BUMP_ON | BUMP_HOOKABLE),
                FLAGS(BossSst, colliderItems[10].info.bumperFlags, BUMP_ON | BUMP_HOOKABLE),
                FLAGS(BossSst, colliderJntSph.base.atFlags, AT_ON),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE) },
    .statics = { { [] { return (void*)BossSst_GetHandStates(); }, 2 * sizeof(s32) } },
    .defeat = [](Actor* actor) {
        for (s16 params : { BONGO_LEFT_HAND, BONGO_RIGHT_HAND }) {
            if (Actor* hand = FindBongoHand(params)) {
                if (GET_PLAYER(gPlayState)->actor.parent == hand) {
                    BossSst_HandReleasePlayer((BossSst*)hand, gPlayState, true);
                }
            }
        }
        actor->colChkInfo.health = 0;
        BossSst_HeadSetupDeath((BossSst*)actor, gPlayState);
        Enemy_StartFinishingBlow(gPlayState, actor);
        // His own hit handler fires the defeat hook, not his death
        GameInteractor_ExecuteOnBossDefeat(actor);
        for (s16 params : { BONGO_LEFT_HAND, BONGO_RIGHT_HAND }) {
            if (Actor* hand = FindBongoHand(params)) {
                BossSst_HandSetupDamage((BossSst*)hand);
            }
        }
        return true;
    },
    .members = { { &sBongoBongoHand, [](Actor*) { return FindBongoHand(BONGO_LEFT_HAND); } },
                 { &sBongoBongoHand, [](Actor*) { return FindBongoHand(BONGO_RIGHT_HAND); } } },
};

// Mini-bosses. Their death runs the rest (drop, switch flag) on every game.

const StateSyncDef sIronKnuckle = {
    .name = "Iron Knuckle",
    .actorId = ACTOR_EN_IK,
    .size = sizeof(EnIk),
    .actionOffset = offsetof(EnIk, actionFunc),
    .fightActions = { ACTION(func_80A747C0), ACTION(func_80A7492C), ACTION(func_80A74BA4), ACTION(func_80A74EBC),
                      ACTION(func_80A7510C), ACTION(func_80A75260), ACTION(func_80A7545C), ACTION(func_80A75530),
                      ACTION(func_80A7567C), ACTION(func_80A758B0) },
    .hitReactions = { ACTION(func_80A758B0) },
    .deathActions = { ACTION(func_80A75A38) },
    // Nabooru's intro and defeat are cutscenes with an update of their own
    .isFighting = [](Actor* actor) {
        return actor->update == func_80A75FA0 && ActionIndex(actor, sIronKnuckle) >= 0 &&
               !(actor->params == 0 && actor->colChkInfo.health <= 10);
    },
    .isDefeated = [](Actor* actor) {
        return ((EnIk*)actor)->actionFunc == func_80A75A38 || (actor->params == 0 && actor->colChkInfo.health <= 10);
    },
    // Woken up by a hit, which only the hitter's game sees
    .wasHit = [](Actor* actor, const void* before) {
        return ((EnIk*)actor)->actionFunc == func_80A747C0 && ((EnIk*)actor)->skelAnime.playSpeed != 0.0f &&
               ((const EnIk*)before)->skelAnime.playSpeed == 0.0f;
    },
    .skelAnimes = { offsetof(EnIk, skelAnime) },
    .fields = { FIELDS(EnIk, unk_2F8, unk_300), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE),
                FIELD(Actor, colorFilterParams), FIELD(Actor, colorFilterTimer) },
    // The armour falls off where the hit that broke it landed, so it's started here too
    .onApply = [](Actor* actor, const void* before) {
        EnIk* ik = (EnIk*)actor;
        if (ik->armorStatusFlag != 0 && ((const EnIk*)before)->armorStatusFlag == 0) {
            BodyBreak_Alloc(&ik->bodyBreak, 3, gPlayState);
        }
    },
    .defeat = [](Actor* actor) {
        if (actor->params == 0) {
            // Nabooru: her own update starts the defeat cutscene
            actor->colChkInfo.health = 10;
            return true;
        }
        actor->colChkInfo.health = 0;
        func_80A7598C((EnIk*)actor);
        Enemy_StartFinishingBlow(gPlayState, actor);
        return true;
    },
};

const StateSyncDef sStalfos = {
    .name = "Stalfos",
    .actorId = ACTOR_EN_TEST,
    .size = sizeof(EnTest),
    .actionOffset = offsetof(EnTest, actionFunc),
    // Not waiting to rise or drop, which each game does when a Link gets near
    .fightActions = { ACTION(EnTest_Idle), ACTION(EnTest_WalkAndBlock), ACTION(func_80860C24), ACTION(func_80860F84),
                      ACTION(EnTest_SlashDown), ACTION(EnTest_SlashDownEnd), ACTION(EnTest_SlashUp),
                      ACTION(EnTest_JumpBack), ACTION(EnTest_Jumpslash), ACTION(EnTest_JumpUp),
                      ACTION(EnTest_StopAndBlock), ACTION(EnTest_IdleFromBlock), ACTION(func_808621D4),
                      ACTION(func_80862418), ACTION(EnTest_Stunned), ACTION(func_808628C8), ACTION(func_80862E6C),
                      ACTION(EnTest_Recoil) },
    .hitReactions = { ACTION(func_808621D4), ACTION(func_80862418), ACTION(EnTest_Stunned) },
    .deathActions = { ACTION(func_80863044), ACTION(func_8086318C), ACTION(func_808633E8) },
    // Its upper body blocks and swings on a skeleton of its own
    .skelAnimes = { offsetof(EnTest, skelAnime), offsetof(EnTest, upperSkelanime) },
    .fields = { FIELD(EnTest, unk_7C8), FIELDS(EnTest, headRot, unk_7DC), FIELDS(EnTest, unk_7DE, lastDamageEffect),
                FIELDS(EnTest, unk_7E4, unk_7EC), FIELD(EnTest, meleeWeaponState), FIELD(Actor, params),
                FIELD(Actor, home.rot.x), FIELD(Actor, scale), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED),
                FIELD(Actor, colorFilterParams), FIELD(Actor, colorFilterTimer) },
    .defeat = [](Actor* actor) {
        actor->colChkInfo.health = 0;
        func_80862FA8((EnTest*)actor, gPlayState);
        Enemy_StartFinishingBlow(gPlayState, actor);
        return true;
    },
};

// Lizalfos and Dinolfos. Dodongo's Cavern's pair take turns to fight, which their overlay keeps in two statics.
const StateSyncDef sLizalfos = {
    .name = "Lizalfos",
    .actorId = ACTOR_EN_ZF,
    .size = sizeof(EnZf),
    .actionOffset = offsetof(EnZf, actionFunc),
    // Not dropping in, which each game does when a Link gets near
    .fightActions = { ACTION(func_80B4543C), ACTION(EnZf_ApproachPlayer), ACTION(EnZf_JumpForward),
                      ACTION(func_80B46098), ACTION(func_80B462E4), ACTION(func_80B463E4), ACTION(EnZf_Slash),
                      ACTION(EnZf_RecoilFromBlockedSlash), ACTION(EnZf_JumpBack), ACTION(EnZf_Stunned),
                      ACTION(EnZf_SheatheSword), ACTION(EnZf_HopAndTaunt), ACTION(EnZf_HopAway),
                      ACTION(EnZf_DrawSword), ACTION(EnZf_Damaged), ACTION(EnZf_JumpUp), ACTION(func_80B483E4),
                      ACTION(EnZf_CircleAroundPlayer) },
    .hitReactions = { ACTION(EnZf_Stunned), ACTION(EnZf_Damaged) },
    .deathActions = { ACTION(EnZf_Die) },
    .skelAnimes = { offsetof(EnZf, skelAnime) },
    .fields = { FIELDS(EnZf, action, hopAnimIndex), FIELDS(EnZf, headRot, alpha), FIELDS(EnZf, unk_408, damageEffect),
                FIELD(Actor, shape.shadowAlpha), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED),
                FIELD(Actor, colorFilterParams), FIELD(Actor, colorFilterTimer) },
    .statics = { { [] { return (void*)EnZf_GetPairState(0); }, sizeof(s16) },
                 { [] { return (void*)EnZf_GetPairState(1); }, sizeof(s16) } },
    .defeat = [](Actor* actor) {
        // Its death sets the switch flag after the second one of the pair
        actor->colChkInfo.health = 0;
        EnZf_SetupDie((EnZf*)actor);
        Enemy_StartFinishingBlow(gPlayState, actor);
        return true;
    },
};

const StateSyncDef sDeadHand = {
    .name = "Dead Hand",
    .actorId = ACTOR_EN_DH,
    .size = sizeof(EnDh),
    .actionOffset = offsetof(EnDh, actionFunc),
    .fightActions = { ACTION(EnDh_Wait), ACTION(EnDh_Walk), ACTION(EnDh_Retreat), ACTION(EnDh_Attack),
                      ACTION(EnDh_Burrow), ACTION(EnDh_Damage) },
    .hitReactions = { ACTION(EnDh_Damage) },
    .deathActions = { ACTION(EnDh_Death) },
    // Its hands tell it through params when they grab someone or die. Each game has its own hands (they grab their
    // own Link), so that goes to the room owner like a hit.
    .wasHit = [](Actor* actor, const void* before) {
        return actor->params != ((const EnDh*)before)->actor.params && actor->params != ENDH_DEATH;
    },
    .skelAnimes = { offsetof(EnDh, skelAnime) },
    .fields = { FIELDS(EnDh, actionState, drawDirtWave), FIELDS(EnDh, alpha, dirtWavePhase),
                FIELDS(EnDh, dirtWaveSpread, dirtWaveAlpha), FIELD(Actor, params), FIELD(Actor, shape.yOffset),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_REACT_TO_LENS),
                FIELD(Actor, colorFilterParams), FIELD(Actor, colorFilterTimer) },
    .defeat = [](Actor* actor) {
        actor->colChkInfo.health = 0;
        EnDh_SetupDeath((EnDh*)actor); // Also sinks the hands
        Enemy_StartFinishingBlow(gPlayState, actor);
        return true;
    },
};

const StateSyncDef sBari = {
    .name = "Bari",
    .actorId = ACTOR_EN_VALI,
    .size = sizeof(EnVali),
    .actionOffset = offsetof(EnVali, actionFunc),
    .fightActions = { ACTION(EnVali_Lurk), ACTION(EnVali_DropAppear), ACTION(EnVali_FloatIdle),
                      ACTION(EnVali_Attacked), ACTION(EnVali_Retaliate), ACTION(EnVali_MoveArmsDown),
                      ACTION(EnVali_Burnt), ACTION(EnVali_Stunned), ACTION(EnVali_Frozen),
                      ACTION(EnVali_ReturnToLurk) },
    .hitReactions = { ACTION(EnVali_Attacked), ACTION(EnVali_Retaliate), ACTION(EnVali_Burnt),
                      ACTION(EnVali_Stunned), ACTION(EnVali_Frozen) },
    .deathActions = { ACTION(EnVali_DivideAndDie) },
    .skelAnimes = { offsetof(EnVali, skelAnime) },
    .fields = { FIELDS(EnVali, lightningTimer, timer), FIELDS(EnVali, armScale, floatHomeHeight),
                FIELD(Actor, params), FLAGS(EnVali, bodyCollider.base.acFlags, AC_ON),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED), FIELD(Actor, colorFilterParams),
                FIELD(Actor, colorFilterTimer) },
    // Hidden on the ceiling until it drops, which shows it
    .onApply = [](Actor* actor, const void* before) {
        if (((EnVali*)actor)->actionFunc != EnVali_Lurk) {
            actor->draw = EnVali_Draw;
        }
    },
    .defeat = [](Actor* actor) {
        // Splits into Biris on every game
        actor->colChkInfo.health = 0;
        EnVali_SetupDivideAndDie((EnVali*)actor, gPlayState);
        return true;
    },
};

const StateSyncDef sWolfos = {
    .name = "Wolfos",
    .actorId = ACTOR_EN_WF,
    .size = sizeof(EnWf),
    .actionOffset = offsetof(EnWf, actionFunc),
    // Not EnWf_WaitToAppear: each game's Wolfos comes out when a Link gets near it there
    .fightActions = { ACTION(EnWf_Wait), ACTION(EnWf_RunAtPlayer), ACTION(EnWf_SearchForPlayer),
                      ACTION(EnWf_RunAroundPlayer), ACTION(EnWf_Slash), ACTION(EnWf_RecoilFromBlockedSlash),
                      ACTION(EnWf_BackflipAway), ACTION(EnWf_Stunned), ACTION(EnWf_Damaged),
                      ACTION(EnWf_SomersaultAndAttack), ACTION(EnWf_Blocking), ACTION(EnWf_Sidestep) },
    .hitReactions = { ACTION(EnWf_Stunned), ACTION(EnWf_Damaged) },
    .deathActions = { ACTION(EnWf_Die) },
    .skelAnimes = { offsetof(EnWf, skelAnime) },
    .fields = { FIELD(EnWf, action), FIELDS(EnWf, unk_2E2, runSpeed), FIELD(EnWf, slashStatus),
                FIELDS(EnWf, runAngle, unk_300), FIELD(EnWf, unk_4D4), FIELD(Actor, scale),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED), FIELD(Actor, colorFilterParams),
                FIELD(Actor, colorFilterTimer) },
    .defeat = [](Actor* actor) {
        actor->colChkInfo.health = 0;
        EnWf_SetupDie((EnWf*)actor);
        Enemy_StartFinishingBlow(gPlayState, actor);
        return true;
    },
};

const StateSyncDef sRedead = {
    .name = "Redead",
    .actorId = ACTOR_EN_RD,
    .size = sizeof(EnRd),
    .actionOffset = offsetof(EnRd, actionFunc),
    // Not rising from a coffin, which each game does when a Link comes near
    .fightActions = { ACTION(EnRd_Idle), ACTION(EnRd_WalkToPlayer), ACTION(EnRd_WalkToHome),
                      ACTION(EnRd_WalkToParent), ACTION(EnRd_Grab), ACTION(EnRd_AttemptPlayerFreeze),
                      ACTION(EnRd_StandUp), ACTION(EnRd_Crouch), ACTION(EnRd_Damaged), ACTION(EnRd_Stunned) },
    .hitReactions = { ACTION(EnRd_Damaged), ACTION(EnRd_Stunned) },
    // Grabbing and freezing are about this game's own Link
    .localActions = { ACTION(EnRd_Grab), ACTION(EnRd_AttemptPlayerFreeze) },
    .deathActions = { ACTION(EnRd_Dead) },
    .skelAnimes = { offsetof(EnRd, skelAnime) },
    // Not the grab and freeze state, nor the parent it mourns (a pointer)
    .fields = { FIELD(EnRd, isMourning), FIELDS(EnRd, timer, upperBodyYRotation),
                FIELDS(EnRd, alpha, sunsSongStunTimer),
                FIELDS(EnRd, stunnedBySunsSong, damageReaction), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED),
                FIELD(Actor, colorFilterParams), FIELD(Actor, colorFilterTimer) },
    .defeat = [](Actor* actor) {
        actor->colChkInfo.health = 0;
        EnRd_SetupDead((EnRd*)actor);
        return true;
    },
};

const StateSyncDef sFreezard = {
    .name = "Freezard",
    .actorId = ACTOR_EN_FZ,
    .size = sizeof(EnFz),
    .actionOffset = offsetof(EnFz, actionFunc),
    .fightActions = { ACTION(EnFz_Wait), ACTION(EnFz_Appear), ACTION(EnFz_AimForMove), ACTION(EnFz_MoveTowardsPlayer),
                      ACTION(EnFz_AimForFreeze), ACTION(EnFz_BlowSmoke), ACTION(EnFz_Disappear),
                      ACTION(EnFz_BlowSmokeStationary) },
    .deathActions = { ACTION(EnFz_Melt), ACTION(EnFz_Despawn) },
    // Not the ice smoke, which each game makes for itself
    .fields = { FIELD(EnFz, posOrigin), FIELD(EnFz, counter), FIELD(EnFz, timer), FIELDS(EnFz, isMoving, isFreezing),
                FIELD(EnFz, iceSmokeFreezingSpawnHeight), FIELDS(EnFz, speedXZ, envAlpha),
                FIELDS(EnFz, state, isDespawning), FIELDS(EnFz, wallHitPos, distToTargetSq),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED), FIELD(Actor, colorFilterParams),
                FIELD(Actor, colorFilterTimer) },
    .defeat = [](Actor* actor) {
        actor->colChkInfo.health = 0;
        EnFz_SetupDespawn((EnFz*)actor, gPlayState);
        return true;
    },
};
// clang-format on

const std::vector<const StateSyncDef*>& StateSyncDefs() {
    static const std::vector<const StateSyncDef*> defs = {
        &sGohma,
        &sKingDodongo,
        // Not Phantom Ganon yet: in a two-player test his horse ended up in another painting on the other game
        // while taking every state (boss-sync-steps.mjs)
        &sVolvagia,
        &sVolvagiaHead,
        &sBongoBongo,
        &sBongoBongoHand,
        &sIronKnuckle,
        &sStalfos,
        &sLizalfos,
        &sDeadHand,
        &sBari,
        &sWolfos,
        &sRedead,
        &sFreezard,
    };
    return defs;
}

} // namespace

// MARK: - Hooks

void Reset() {
    sScene = -1;
    sRoom = -1;
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

uint32_t RoomFrames() {
    return sFrame - sRoomSince;
}

void OnFrame() {
    if (!IsEnabled()) {
        return;
    }
    sFrame++;
    CheckScene();
    if (gPlayState->roomCtx.curRoom.num != sRoom) {
        sRoom = gPlayState->roomCtx.curRoom.num;
        sRoomSince = sFrame;
    }
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
    if (synced == nullptr) {
        return;
    }
    if (RoomOwner(KeyRoom(key)) != sender) {
        synced->dropped["not owner"]++;
        return;
    }
    // A late packet mustn't undo a newer one. One from long before is the room owner starting over (its copy of the
    // actor spawned again).
    uint32_t seq = payload.value("seq", 0u);
    if (seq <= synced->seq && sender == synced->seqFrom && synced->seq - seq < SYNC_SEQ_RESTART) {
        synced->dropped["old"]++;
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
            synced->dropped["hold"]++;
            return;
        }
        synced->holdUntil = 0;
    }

    // Anything this frame did goes out before the room owner's state replaces it
    CheckSynced(key, *synced);
    Actor* leader = Leader(*synced);
    if (sFrame < synced->holdUntil || !IsFighting(leader, *synced->def) || IsDefeated(leader, *synced->def)) {
        synced->dropped[sFrame < synced->holdUntil ? "hit" : "not fighting"]++;
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
    { "King Dodongo", 0, offsetof(BossDodongo, collider) },
    { "Phantom Ganon", 0, offsetof(BossGanondrof, colliderBody) },
    { "Volvagia", 0, offsetof(BossFd, collider) },
    { "Volvagia", 1, offsetof(BossFd2, collider) },
    { "Bongo Bongo", 0, offsetof(BossSst, colliderCyl) },
    { "Bongo Bongo", 1, offsetof(BossSst, colliderJntSph) },
    { "Bongo Bongo", 2, offsetof(BossSst, colliderJntSph) },
    { "Iron Knuckle", 0, offsetof(EnIk, bodyCollider) },
    { "Stalfos", 0, offsetof(EnTest, bodyCollider) },
    { "Lizalfos", 0, offsetof(EnZf, bodyCollider) },
    { "Dead Hand", 0, offsetof(EnDh, collider2) },
    { "Bari", 0, offsetof(EnVali, bodyCollider) },
    { "Wolfos", 0, offsetof(EnWf, colliderCylinderBody) },
    { "Redead", 0, offsetof(EnRd, collider) },
    { "Freezard", 0, offsetof(EnFz, collider1) },
};

// Things that happen to a fight only on the game of the player who did them, other than collider hits
struct DebugPoke {
    const char* name;
    int member;
    bool (*poke)(Actor* actor); // false if it can't happen right now
};

const DebugPoke sDebugPokes[] = {
    // Swallows a bomb
    { "King Dodongo", 0,
      [](Actor* actor) {
          BossDodongo* kd = (BossDodongo*)actor;
          if (kd->actionFunc != BossDodongo_Inhale) {
              return false;
          }
          BossDodongo_SetupExplode(kd);
          return true;
      } },
    // Hanging from the ceiling out of the camera's view, it doesn't update; a player walking up would look at it
    { "Bari", 0,
      [](Actor* actor) {
          actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
          return true;
      } },
    // One of its hands grabs this game's Link
    { "Dead Hand", 0,
      [](Actor* actor) {
          if (((EnDh*)actor)->actionFunc != EnDh_Wait || actor->params != ENDH_WAIT_UNDERGROUND) {
              return false;
          }
          actor->params = ENDH_START_ATTACK_GRAB;
          return true;
      } },
};

// The def name in "name" or "name@index"
std::string DefName(const char* name) {
    std::string defName = name;
    return defName.substr(0, defName.find('@'));
}

// A state-synced actor that leads its group, by its def's name: the first one, or "name@index" for the one with
// that spawn list index
Actor* FindSyncedLeader(const char* name, const StateSyncDef** def) {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    const char* at = strchr(name, '@');
    int index = at != nullptr ? atoi(at + 1) : -1;
    for (int category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[category].head; actor != nullptr; actor = actor->next) {
            const StateSyncDef* found = FindDef(actor);
            Key key;
            if (actor->update != nullptr && found != nullptr && !found->isMember && DefName(name) == found->name &&
                (index < 0 || (GetKey(actor, &key) && (s16)(key & 0xFFFF) == index))) {
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

// Who runs the enemies of a room of this scene (0 without a game)
EMSCRIPTEN_KEEPALIVE int web_debug_room_owner(int room) {
    if (gPlayState == nullptr || Anchor::Instance == nullptr) {
        return 0;
    }
    return (int)RoomOwner((s8)room);
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
                auto synced = sSynced.find(key);
                if (synced != sSynced.end()) {
                    nlohmann::json taken = nlohmann::json::object();
                    for (auto& [clientId, hit] : synced->second.hitsTaken) {
                        taken[std::to_string(clientId)] = hit;
                    }
                    entry["sync"] = { { "seq", synced->second.seq },
                                      { "applied", synced->second.applied },
                                      { "sent", synced->second.sentSeq },
                                      { "from", synced->second.seqFrom },
                                      { "dropped", synced->second.dropped },
                                      { "skipped", synced->second.skipped },
                                      { "actors", synced->second.actors.size() },
                                      { "hits", synced->second.hitSeq },
                                      { "holding", sFrame < synced->second.holdUntil },
                                      { "taken", taken } };
                }
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
        if (DefName(name) != target.name || target.member != member) {
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

// Something only the player who does it sees (sDebugPokes), e.g. King Dodongo swallowing a bomb
EMSCRIPTEN_KEEPALIVE int web_debug_synced_poke(const char* name, int member) {
    const StateSyncDef* def;
    Actor* actor = FindSyncedMember(name, member, &def);
    for (const DebugPoke& poke : sDebugPokes) {
        if (actor != nullptr && DefName(name) == poke.name && poke.member == member) {
            return poke.poke(actor);
        }
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
    return def->defeat(actor);
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

// The height of the floor below a point, and whether it's a pit Link would void out in (-32000 for no floor)
EMSCRIPTEN_KEEPALIVE const char* web_debug_floor(float x, float y, float z) {
    static std::string out;
    CollisionPoly* poly = nullptr;
    s32 bgId;
    Vec3f pos = { x, y, z };
    f32 floorY = BgCheck_EntityRaycastFloor5(gPlayState, &gPlayState->colCtx, &poly, &bgId, nullptr, &pos);
    // The floor properties Link voids out on (see Player_HandleExitsAndVoids)
    u32 property = poly != nullptr ? func_80041EA4(&gPlayState->colCtx, poly, bgId) : 0;
    bool pit = property == FLOOR_PROPERTY_5 || property == FLOOR_PROPERTY_12;
    out = nlohmann::json({ { "y", floorY }, { "pit", pit } }).dump();
    return out.c_str();
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
