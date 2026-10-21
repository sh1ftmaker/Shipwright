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
#include "src/overlays/actors/ovl_Boss_Va/z_boss_va.h"
#include "src/overlays/actors/ovl_Boss_Mo/z_boss_mo.h"
#include "src/overlays/actors/ovl_Boss_Tw/z_boss_tw.h"
#include "src/overlays/actors/ovl_Boss_Ganon/z_boss_ganon.h"
#include "src/overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
#include "src/overlays/actors/ovl_En_Hy/z_en_hy.h"
#include "src/overlays/actors/ovl_En_Daiku_Kakariko/z_en_daiku_kakariko.h"
#include "src/overlays/actors/ovl_En_Cs/z_en_cs.h"
#include "src/overlays/actors/ovl_En_Mm/z_en_mm.h"
#include "src/overlays/actors/ovl_En_Dog/z_en_dog.h"
#include "src/overlays/actors/ovl_En_Horse_Normal/z_en_horse_normal.h"
#include "src/overlays/actors/ovl_Obj_Tsubo/z_obj_tsubo.h"
#include "src/overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "src/overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "src/overlays/actors/ovl_Bg_Ganon_Otyuka/z_bg_ganon_otyuka.h"
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
void BossVa_BodyPhase1(BossVa* va, PlayState* play);
void BossVa_BodyPhase2(BossVa* va, PlayState* play);
void BossVa_BodyPhase3(BossVa* va, PlayState* play);
void BossVa_BodyPhase4(BossVa* va, PlayState* play);
void BossVa_BodyDeath(BossVa* va, PlayState* play);
void BossVa_SetupBodyDeath(BossVa* va, PlayState* play);
void BossVa_SupportAttached(BossVa* va, PlayState* play);
void BossVa_SupportCut(BossVa* va, PlayState* play);
void BossVa_SetupSupportCut(BossVa* va, PlayState* play);
void BossVa_BariPhase2Attack(BossVa* va, PlayState* play);
void BossVa_BariPhase3Attack(BossVa* va, PlayState* play);
void BossVa_BariPhase3Stunned(BossVa* va, PlayState* play);
void BossVa_BariDeath(BossVa* va, PlayState* play);
void BossVa_KillBari(BossVa* va, PlayState* play);
void* BossVa_GetSyncStatic(s32 which);
Actor* BossVa_SpawnPart(BossVa* body, PlayState* play, s16 params);
BossMo* BossMo_GetTentacle(s32 which);
void BossTw_FlyTo(BossTw* tw, PlayState* play);
void BossTw_TurnToPlayer(BossTw* tw, PlayState* play);
void BossTw_ShootBeam(BossTw* tw, PlayState* play);
void BossTw_FinishBeamShoot(BossTw* tw, PlayState* play);
void BossTw_HitByBeam(BossTw* tw, PlayState* play);
void BossTw_Laugh(BossTw* tw, PlayState* play);
void BossTw_Spin(BossTw* tw, PlayState* play);
void BossTw_Wait(BossTw* tw, PlayState* play);
void BossTw_MergeCS(BossTw* tw, PlayState* play);
void BossTw_DeathCS(BossTw* tw, PlayState* play);
void BossTw_TwinrovaMergeCS(BossTw* tw, PlayState* play);
void BossTw_TwinrovaFly(BossTw* tw, PlayState* play);
void BossTw_TwinrovaArriveAtTarget(BossTw* tw, PlayState* play);
void BossTw_TwinrovaChargeBlast(BossTw* tw, PlayState* play);
void BossTw_TwinrovaShootBlast(BossTw* tw, PlayState* play);
void BossTw_TwinrovaDoneBlastShoot(BossTw* tw, PlayState* play);
void BossTw_TwinrovaStun(BossTw* tw, PlayState* play);
void BossTw_TwinrovaGetUp(BossTw* tw, PlayState* play);
void BossTw_TwinrovaSpin(BossTw* tw, PlayState* play);
void BossTw_TwinrovaLaugh(BossTw* tw, PlayState* play);
void BossTw_TwinrovaDeathCS(BossTw* tw, PlayState* play);
void BossTw_TwinrovaSetupDeathCS(BossTw* tw, PlayState* play);
void BossTw_SetupShootBeam(BossTw* tw, PlayState* play);
void BossTw_SetupHitByBeam(BossTw* tw, PlayState* play);
BossTw* BossTw_GetSister(s32 which);
u8* BossTw_GetBlastState(s32 which);
void BossGanon_Wait(BossGanon* gdf, PlayState* play);
void BossGanon_ChargeLightBall(BossGanon* gdf, PlayState* play);
void BossGanon_PlayTennis(BossGanon* gdf, PlayState* play);
void BossGanon_PoundFloor(BossGanon* gdf, PlayState* play);
void BossGanon_ChargeBigMagic(BossGanon* gdf, PlayState* play);
void BossGanon_Block(BossGanon* gdf, PlayState* play);
void BossGanon_HitByLightBall(BossGanon* gdf, PlayState* play);
void BossGanon_Vulnerable(BossGanon* gdf, PlayState* play);
void BossGanon_Damaged(BossGanon* gdf, PlayState* play);
void BossGanon_DeathAndTowerCutscene(BossGanon* gdf, PlayState* play);
void BossGanon_SetupDeathCutscene(BossGanon* gdf, PlayState* play);
void BossGanon_SetupHitByLightBall(BossGanon* gdf, PlayState* play);
void func_808FFDB0(BossGanon2* ganon, PlayState* play); // Back to guarding (waits for its object)
void func_808FFEBC(BossGanon2* ganon, PlayState* play); // Guard
void func_808FFFE0(BossGanon2* ganon, PlayState* play); // Reflected a hit
void func_80900104(BossGanon2* ganon, PlayState* play); // Stunned by a light arrow
void func_8090026C(BossGanon2* ganon, PlayState* play); // Damaged
void func_80900344(BossGanon2* ganon, PlayState* play); // Walk or run
void func_80900650(BossGanon2* ganon, PlayState* play); // Sword swing
void func_80900890(BossGanon2* ganon, PlayState* play); // Downed (local: each player picks up their own sword)
void func_8090120C(BossGanon2* ganon, PlayState* play); // Defeated
void func_80900818(BossGanon2* ganon, PlayState* play); // Setup downed
void func_80901020(BossGanon2* ganon, PlayState* play); // Setup defeated
void func_809000A0(BossGanon2* ganon, PlayState* play); // Setup stunned
void EnHy_Walk(EnHy* hy, PlayState* play);
void EnHy_Pace(EnHy* hy, PlayState* play);
void EnDaikuKakariko_Wait(EnDaikuKakariko* daiku, PlayState* play);
void EnDaikuKakariko_Run(EnDaikuKakariko* daiku, PlayState* play);
void EnDaikuKakariko_StopRunning(EnDaikuKakariko* daiku, PlayState* play);
void EnCs_Walk(EnCs* cs, PlayState* play);
void EnCs_Wait(EnCs* cs, PlayState* play);
void func_80AAE294(EnMm* mm, PlayState* play); // Running Man: run
void func_80AAE224(EnMm* mm, PlayState* play); // Sit down
void func_80AAE50C(EnMm* mm, PlayState* play); // Get up
void func_80AAE598(EnMm* mm, PlayState* play); // Wait
void EnDog_FollowPath(EnDog* dog, PlayState* play);
void EnDog_ChooseMovement(EnDog* dog, PlayState* play);
void EnDog_Wait(EnDog* dog, PlayState* play);
void ObjTsubo_AirBreak(ObjTsubo* pot, PlayState* play);
void EnKusa_SpawnFragments(EnKusa* bush, PlayState* play);
void EnKusa_SetupCut(EnKusa* bush);
void EnIshi_SpawnFragmentsSmall(EnIshi* rock, PlayState* play);
void EnIshi_SpawnFragmentsLarge(EnIshi* rock, PlayState* play);
void BgGanonOtyuka_WaitToFall(BgGanonOtyuka* tile, PlayState* play);
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
static const std::string PROP_BREAK = "PROP_BREAK";

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
std::unordered_set<Actor*> sSeenActors; // Updated with their key at least once (see CheckNewActor)

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

// The game doesn't update an enemy outside the camera's view, so one the room owner has flying at this player would
// stay frozen (and unseen) wherever it was last in view here. Move those along too, so they come into view where the
// room owner has them.
void FollowRoomOwnerOffscreen() {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != nullptr; actor = actor->next) {
        Key key;
        if (actor->update == nullptr ||
            (actor->flags & (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_INSIDE_CULLING_VOLUME)) ||
            !IsSyncedEnemy(actor) || !GetKey(actor, &key) || sDead.contains(key) || OwnsRoom(actor->room)) {
            continue;
        }
        FollowRoomOwner(actor, key);
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

// MARK: - Pots, bushes and rocks

/**
 * One player breaking a pot, cutting a bush or picking either (or a rock) up takes it away on everyone's game, with
 * its effect but without its drop, which is the breaker's. Like a dead enemy it stays gone while anyone is in its
 * room. A bush that grows back is only cut, so it grows back on everyone's game. The floor tiles in Ganondorf's
 * room fall on everyone's game when they fall on one (his pound, a light ball or a bomb), instead of each game
 * dropping its own.
 */
std::unordered_map<Actor*, Key> sBreakableKeys;    // From before the frame's update: picking one up takes it out of
std::unordered_map<Actor*, bool> sBreakableWasCut; // its room, which its key needs
std::unordered_set<Key> sGone;                     // Broken or taken by anyone this visit
std::unordered_set<Actor*> sTakenHere;             // ...the ones this game's Link is carrying or threw

bool IsBreakable(Actor* actor) {
    return actor->id == ACTOR_OBJ_TSUBO || actor->id == ACTOR_EN_KUSA || actor->id == ACTOR_EN_ISHI ||
           actor->id == ACTOR_BG_GANON_OTYUKA;
}

// Cut (a bush that grows back) or falling (a floor tile): broken, but still there for a while
bool IsCut(Actor* actor) {
    return (actor->id == ACTOR_EN_KUSA && (actor->flags & ACTOR_FLAG_GRASS_DESTROYED)) ||
           (actor->id == ACTOR_BG_GANON_OTYUKA && ((BgGanonOtyuka*)actor)->isFalling);
}

void SendBreak(Key key, bool cut, bool effect) {
    nlohmann::json payload;
    payload["type"] = PROP_BREAK;
    payload["props"] = nlohmann::json::array({ key });
    payload["cut"] = cut;
    payload["effect"] = effect;
    SendToPeers(payload);
}

// What breaking it does on the breaker's game, less the drop
void BreakHere(Actor* actor, bool cut, bool effect) {
    if (effect) {
        switch (actor->id) {
            case ACTOR_OBJ_TSUBO:
                ObjTsubo_AirBreak((ObjTsubo*)actor, gPlayState);
                SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_POT_BROKEN);
                break;
            case ACTOR_EN_KUSA:
                EnKusa_SpawnFragments((EnKusa*)actor, gPlayState);
                SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_PLANT_BROKEN);
                break;
            case ACTOR_EN_ISHI:
                if ((actor->params & 1) == ROCK_LARGE) {
                    EnIshi_SpawnFragmentsLarge((EnIshi*)actor, gPlayState);
                } else {
                    EnIshi_SpawnFragmentsSmall((EnIshi*)actor, gPlayState);
                }
                SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, NA_SE_EV_ROCK_BROKEN);
                break;
        }
    }
    if (actor->id == ACTOR_BG_GANON_OTYUKA) {
        // Its own code makes it fall (with the flash that warns of it) once it's told to
        BgGanonOtyuka* tile = (BgGanonOtyuka*)actor;
        if (cut && tile->actionFunc == BgGanonOtyuka_WaitToFall) {
            tile->isFalling = true;
        }
    } else if (cut) {
        // A bush that grows back: cut, the way its own code cuts it
        EnKusa_SetupCut((EnKusa*)actor);
        actor->flags |= ACTOR_FLAG_GRASS_DESTROYED;
    } else {
        Actor_Kill(actor);
    }
}

void BreakablePreUpdate(Actor* actor, Key key, bool* should) {
    if (sGone.contains(key) && !sTakenHere.contains(actor)) {
        // Someone else broke or took it this visit
        Actor_Kill(actor);
        *should = false;
        return;
    }
    sBreakableKeys[actor] = key;
    // As it was after its last update: something else can break it in between (Ganondorf drops floor tiles)
    sBreakableWasCut.emplace(actor, IsCut(actor));
}

void BreakablePostUpdate(Actor* actor) {
    auto it = sBreakableKeys.find(actor);
    if (it == sBreakableKeys.end()) {
        return;
    }
    Key key = it->second;
    bool gone = actor->update == nullptr || actor->room == -1; // Broken, or picked up
    if (gone && actor->id == ACTOR_BG_GANON_OTYUKA) {
        // A floor tile that has fallen, which it does on everyone's game
        sBreakableKeys.erase(it);
    } else if (gone) {
        sBreakableKeys.erase(it);
        if (actor->update != nullptr) {
            sTakenHere.insert(actor);
        }
        if (sGone.insert(key).second) {
            SendBreak(key, false, true);
        }
    } else if (IsCut(actor) && !sBreakableWasCut[actor]) {
        SendBreak(key, true, true);
    }
    sBreakableWasCut[actor] = IsCut(actor);
}

// They come back when their room loads again, once nobody is left in it
void ForgetGoneInEmptyRooms() {
    std::erase_if(sGone, [](Key key) { return !IsRoomOccupied(KeyRoom(key)); });
}

void SendGoneList() {
    if (sGone.empty()) {
        return;
    }
    nlohmann::json list = nlohmann::json::array();
    for (Key key : sGone) {
        list.push_back(key);
    }
    nlohmann::json payload;
    payload["type"] = PROP_BREAK;
    payload["props"] = list;
    payload["cut"] = false;
    payload["effect"] = false;
    SendToPeers(payload);
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
    bool onlyUp = false; // A count that only goes up (one byte): never set back to a lower value
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
    // Parts that come and go during the fight: made when the room owner has one this game doesn't (a transition the
    // room owner's state skipped here), and taken away when the room owner's is gone but this game's still fights
    void (*spawn)(Actor* leader) = nullptr;
    void (*remove)(Actor* member, Actor* leader) = nullptr;
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
    bool (*isLocal)(Actor* actor) = nullptr;      // The same, for actors that don't keep their state in an action
    // Starts a local action here when the room owner starts one (a beam each game aims at its own Link); without
    // it, the room owner's local actions are just skipped
    void (*enterLocal)(Actor* actor) = nullptr;
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
    nlohmann::json (*debug)(Actor* actor) = nullptr; // For tests: more about its state
    // A townsperson, animal and the like: left alone while this game's Link talks to it, holds it or is in a
    // cutscene (they're all this game's own), goes after nobody else's Link, and is sent less often
    bool npc = false;
    u8 sendEvery = 1; // Frames between states the room owner sends
    void (*beforeSnapshot)(Actor* actor) = nullptr; // Room owner: before its state goes out (to note what it's doing)
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
    uint32_t updates = 0;                              // For tests: frames the leader was updated
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

Actor* sDebugBusy = nullptr; // For tests: as if this game's Link were talking to it

// This game's Link is talking to it, holding it or in a cutscene with it (or any cutscene)
bool IsBusyHere(Actor* actor) {
    Player* player = GET_PLAYER(gPlayState);
    return actor == sDebugBusy || (player->talkActor == actor && (player->stateFlags1 & PLAYER_STATE1_TALKING)) ||
           player->heldActor == actor || actor->parent == &player->actor || player->csActor == actor ||
           Player_InCsMode(gPlayState);
}

bool IsFighting(Actor* actor, const StateSyncDef& def) {
    if (def.npc && IsBusyHere(actor)) {
        return false;
    }
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
        u8* at = (u8*)field.get();
        if (field.onlyUp) {
            *at = MAX(*at, *in);
            in += field.size;
        } else {
            get(at, field.size);
        }
    }
}

// What a def's statics hold right now, to put back after calling the overlay's own code for a transition the room
// owner's state skipped here (which changes them as well)
std::vector<u8> SaveStatics(const StateSyncDef& def) {
    std::vector<u8> out;
    for (const SyncStatic& field : def.statics) {
        out.insert(out.end(), (u8*)field.get(), (u8*)field.get() + field.size);
    }
    return out;
}

void RestoreStatics(const StateSyncDef& def, const std::vector<u8>& saved) {
    const u8* in = saved.data();
    for (const SyncStatic& field : def.statics) {
        memcpy(field.get(), in, field.size);
        in += field.size;
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
        if (actor == nullptr || actor->update == nullptr || ActionIndex(actor, def) < 0 || !IsFighting(actor, def)) {
            members.push_back(nullptr);
        } else {
            if (def.beforeSnapshot != nullptr) {
                def.beforeSnapshot(actor);
            }
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
        bool alive = actor != nullptr && actor->update != nullptr;
        if (i > 0 && Leader(synced) != nullptr) {
            const GroupMember& member = synced.def->members[i - 1];
            // (Found again first: a packet that came in the same frame may have made it already)
            if (!alive && members[i].is_array() && member.spawn != nullptr && member.find(synced.actors[0]) == nullptr) {
                member.spawn(synced.actors[0]);
            } else if (alive && members[i].is_null() && member.remove != nullptr && IsFighting(actor, def)) {
                member.remove(actor, synced.actors[0]);
                continue;
            }
        }
        if (!alive || !members[i].is_array()) {
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
        bool localHere = IsIn(def.localActions, ActionOf(actor, def)) || (def.isLocal != nullptr && def.isLocal(actor)) ||
                         (def.npc && IsBusyHere(actor));
        if (localHere || (def.actionOffset != NO_ACTION && IsIn(def.localActions, def.fightActions[action]))) {
            // Its health is still everyone's
            s32 remoteHealth = ReadHealthAt(data.data() + sizeof(ActorBase), def);
            if (def.healthFollowsOwner || remoteHealth < ReadHealth(actor, def)) {
                WriteHealth(actor, def, remoteHealth);
            }
            if (!localHere && def.enterLocal != nullptr) {
                def.enterLocal(actor);
            }
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

// A peer is in this room, so someone needs its state
bool RoomHasPeer(s8 room) {
    for (auto& [clientId, client] : Anchor::Instance->clients) {
        if (IsPeer(client) && client.curRoomNum == room) {
            return true;
        }
    }
    return false;
}

// The room owner's state for one fight, added to the frame's BOSS_STATE (one packet for all of them)
void AddSyncedState(Key key, Synced& synced, nlohmann::json& states) {
    Actor* leader = Leader(synced);
    if (leader == nullptr || !OwnsRoom(leader->room) || !RoomHasPeer(leader->room) ||
        !IsFighting(leader, *synced.def) || IsDefeated(leader, *synced.def) ||
        sFrame % synced.def->sendEvery != 0) {
        return;
    }
    nlohmann::json payload;
    payload["key"] = key;
    payload["seq"] = ++synced.sentSeq;
    payload["members"] = GroupSnapshot(synced);
    if (!synced.hitsTaken.empty()) {
        nlohmann::json acks = nlohmann::json::array();
        for (auto& [clientId, hit] : synced.hitsTaken) {
            acks.push_back({ clientId, hit });
        }
        payload["acks"] = acks;
    }
    states.push_back(std::move(payload));
}

void StartDefeat(Synced& synced, Actor* leader) {
    synced.pendingDefeat = synced.def->defeat != nullptr && !synced.def->defeat(leader);
}

void SyncedPreUpdate(Actor* actor, Key key, const StateSyncDef& def) {
    Synced& synced = sSynced[key];
    synced.updates++;
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

// Phantom Ganon's horse, which carries him through the paintings. Which painting comes next is random, so only the
// room owner picks it: elsewhere the horse waits in the painting it went into until the room owner's state brings it
// out of the next one. The decoy that rides out of another painting at the same time is spawned on every game from
// the room owner's choice, noted in a few bytes the horse doesn't use (0xFF: none).
constexpr u8 PG_NO_DECOY = 0xFF;
constexpr u8 PG_PAINTINGS = 6;

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
                FIELD(Actor, scale), FIELD(EnfHG, unk_1E2) },
    .onApply = [](Actor* actor, const void* before) {
        EnfHG* horse = (EnfHG*)actor;
        if (OwnsRoom(actor->room)) {
            return; // A hit from someone else
        }
        if (horse->actionFunc == EnfHG_Retreat && horse->timers[0] < 10) {
            // Not picking a painting of its own: the room owner's next state says which
            horse->timers[0] = 10;
        }
        if (horse->actionFunc == EnfHG_Approach && ((const EnfHG*)before)->actionFunc != EnfHG_Approach &&
            (u8)horse->unk_1E2[0] < PG_PAINTINGS) {
            Actor_SpawnAsChild(&gPlayState->actorCtx, actor, gPlayState, ACTOR_BOSS_GANONDROF, actor->world.pos.x,
                               actor->world.pos.y, actor->world.pos.z, 0, 0, 0,
                               (u8)horse->unk_1E2[0] + GND_FAKE_BOSS);
        }
    },
    .isMember = true,
    .debug = [](Actor* actor) {
        EnfHG* horse = (EnfHG*)actor;
        return nlohmann::json{ { "painting", horse->curPainting }, { "target", horse->targetPainting },
                               { "timer", horse->timers[0] }, { "hitTimer", horse->hitTimer },
                               { "decoy", (u8)horse->unk_1E2[0] } };
    },
    // The decoy riding out with it, if any
    .beforeSnapshot = [](Actor* actor) {
        ((EnfHG*)actor)->unk_1E2[0] = (char)PG_NO_DECOY;
        for (Actor* it = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; it != nullptr; it = it->next) {
            if (it->id == ACTOR_BOSS_GANONDROF && it->params >= GND_FAKE_BOSS && it->parent == actor &&
                it->update != nullptr) {
                ((EnfHG*)actor)->unk_1E2[0] = (char)(it->params - GND_FAKE_BOSS);
            }
        }
    },
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
    // His own horse, by what it is rather than his child pointer, which in the middle of a frame can be the decoy's
    .members = { { &sPhantomGanonHorse,
                   [](Actor* pg) {
                       for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BG].head; actor != nullptr;
                            actor = actor->next) {
                           if (actor->id == ACTOR_EN_FHG && actor->params < GND_FAKE_BOSS && actor->parent == pg &&
                               actor->update != nullptr) {
                               return actor;
                           }
                       }
                       return (Actor*)nullptr;
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
    .debug = [](Actor* actor) {
        Player* player = GET_PLAYER(gPlayState);
        return nlohmann::json{ { "vy", actor->velocity.y },
                               { "link1", player->stateFlags1 },
                               { "link2", player->stateFlags2 },
                               { "floor", actor->floorHeight },
                               { "flags", actor->flags },
                               { "drawn", actor->isDrawn },
                               { "freeze", actor->freezeTimer },
                               { "dist", actor->xzDistToPlayer } };
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
// Barinade: the body (in the spawn list), the three tethers holding it to the ceiling and the ten Bari that circle it,
// which the body spawns as the fight goes on. Where the fight is (a count of what's been cut and killed), the body's
// stun and phase 4's health are statics of the overlay. The zappers aim at each game's own Link, so they're local.
constexpr u8 BOSSVA_PHASE_DEATH = 18;

Actor* FindBarinadePart(Actor* body, s16 params) {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; actor != nullptr; actor = actor->next) {
        if (actor->id == ACTOR_BOSS_VA && actor->params == params && actor->parent == body && actor->update != nullptr) {
            return actor;
        }
    }
    return nullptr;
}

// Runs some of the overlay's own code for a transition, keeping the statics as the room owner sent them
template <typename F> void KeepBarinadeStatics(F run) {
    static const u8 sizes[] = { 1, 1, 2, 1, 1 };
    u8 saved[5][2];
    for (int i = 0; i < 5; i++) {
        memcpy(saved[i], BossVa_GetSyncStatic(i), sizes[i]);
    }
    run();
    for (int i = 0; i < 5; i++) {
        memcpy(BossVa_GetSyncStatic(i), saved[i], sizes[i]);
    }
}

const StateSyncDef sBarinadeTether = {
    .name = "Barinade's tether",
    .actorId = ACTOR_BOSS_VA,
    .size = sizeof(BossVa),
    .match = [](Actor* actor) { return actor->params >= BOSSVA_SUPPORT_1 && actor->params <= BOSSVA_SUPPORT_3; },
    .actionOffset = offsetof(BossVa, actionFunc),
    .fightActions = { ACTION(BossVa_SupportAttached), ACTION(BossVa_SupportCut) },
    .hitReactions = { ACTION(BossVa_SupportCut) },
    // Hangs from the body, which places it each frame. Cut on the room owner's game: cut here the same way (its stump,
    // the cut skeleton). Never joined up again.
    .onApply = [](Actor* actor, const void* before) {
        BossVa* tether = (BossVa*)actor;
        if (((const BossVa*)before)->actionFunc == BossVa_SupportCut) {
            tether->actionFunc = BossVa_SupportCut;
        } else if (tether->actionFunc == BossVa_SupportCut) {
            tether->actionFunc = BossVa_SupportAttached;
            KeepBarinadeStatics([&] { BossVa_SetupSupportCut(tether, gPlayState); });
        }
    },
    .isMember = true,
};

const StateSyncDef sBarinadeBari = {
    .name = "Barinade's Bari",
    .actorId = ACTOR_BOSS_VA,
    .size = sizeof(BossVa),
    .match = [](Actor* actor) { return actor->params >= BOSSVA_BARI_UPPER_1 && actor->params <= BOSSVA_BARI_LOWER_5; },
    .actionOffset = offsetof(BossVa, actionFunc),
    .fightActions = { ACTION(BossVa_BariPhase2Attack), ACTION(BossVa_BariPhase3Attack),
                      ACTION(BossVa_BariPhase3Stunned) },
    .hitReactions = { ACTION(BossVa_BariDeath) }, // Killed here
    .deathActions = { ACTION(BossVa_BariDeath) },
    // They circle the body at a radius and speed of their own
    .fields = { FIELDS(BossVa, timer, timer2), FIELDS(BossVa, unk_1A0, unk_1A4), FIELD(BossVa, unk_1AC),
                FIELD(BossVa, unk_1F0), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED),
                FIELD(Actor, colorFilterParams), FIELD(Actor, colorFilterTimer) },
    .isMember = true,
};

template <s16 P> Actor* FindBarinadePartP(Actor* body) {
    return FindBarinadePart(body, P);
}

// A Bari the room owner has: spawned here when this game skipped the transition that spawns it
template <s16 P> void SpawnBarinadeBari(Actor* body) {
    if (((BossVa*)body)->actionFunc != BossVa_BodyPhase1) {
        BossVa_SpawnPart((BossVa*)body, gPlayState, P);
    }
}

// One the room owner killed: killed here too (which counts it, as the room owner's statics already have)
void RemoveBarinadeBari(Actor* bari, Actor* body) {
    if (((BossVa*)body)->actionFunc != BossVa_BodyPhase1) {
        KeepBarinadeStatics([&] { BossVa_KillBari((BossVa*)bari, gPlayState); });
    }
}

#define BARINADE_BARI(p) { &sBarinadeBari, FindBarinadePartP<p>, SpawnBarinadeBari<p>, RemoveBarinadeBari }

const StateSyncDef sBarinade = {
    .name = "Barinade",
    .actorId = ACTOR_BOSS_VA,
    .size = sizeof(BossVa),
    .match = [](Actor* actor) { return actor->params == BOSSVA_BODY; },
    .actionOffset = offsetof(BossVa, actionFunc),
    .fightActions = { ACTION(BossVa_BodyPhase1), ACTION(BossVa_BodyPhase2), ACTION(BossVa_BodyPhase3),
                      ACTION(BossVa_BodyPhase4) },
    .deathActions = { ACTION(BossVa_BodyDeath) },
    // A hit on the body changes neither its action nor its health: it stuns it (a colour filter) or, in phase 4,
    // makes it invincible for a moment while a static counts down
    .wasHit = [](Actor* actor, const void* before) {
        const BossVa* was = (const BossVa*)before;
        BossVa* body = (BossVa*)actor;
        return body->actionFunc != BossVa_BodyPhase1 && (actor->colorFilterTimer > was->actor.colorFilterTimer ||
                                                         body->invincibilityTimer > was->invincibilityTimer);
    },
    .skelAnimes = { offsetof(BossVa, skelAnime) },
    .fields = { FIELDS(BossVa, invincibilityTimer, timer2), FIELDS(BossVa, unk_1A0, unk_1B0), FIELD(BossVa, headRot),
                FIELD(BossVa, colliderBody.info.bumper.dmgFlags), FIELD(BossVa, colliderBody.dim.radius),
                FIELD(Actor, shape.yOffset), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE),
                FIELD(Actor, colorFilterParams), FIELD(Actor, colorFilterTimer) },
    // Not the Bari a sword hit on the body kills next (a count each game spends on its own Bari)
    .statics = { { [] { return BossVa_GetSyncStatic(0); }, 1, true },
                 { [] { return BossVa_GetSyncStatic(1); }, 1 },
                 { [] { return BossVa_GetSyncStatic(2); }, 2 },
                 { [] { return BossVa_GetSyncStatic(3); }, 1 },
                 { [] { return BossVa_GetSyncStatic(4); }, 1 } },
    // Its phases only go forward; the change to the next one also spawns Bari, which a late state mustn't repeat
    .onApply = [](Actor* actor, const void* before) {
        if (ActionIndex((Actor*)before, sBarinade) > ActionIndex(actor, sBarinade)) {
            ((BossVa*)actor)->actionFunc = ((const BossVa*)before)->actionFunc;
        }
    },
    .defeat = [](Actor* actor) {
        // What the fight did before its end: every tether cut (their deaths are part of the cutscene), every Bari dead
        for (s16 params = BOSSVA_SUPPORT_1; params <= BOSSVA_SUPPORT_3; params++) {
            BossVa* tether = (BossVa*)FindBarinadePart(actor, params);
            if (tether != nullptr && tether->actionFunc == BossVa_SupportAttached) {
                BossVa_SetupSupportCut(tether, gPlayState);
            }
        }
        for (s16 params = BOSSVA_BARI_UPPER_1; params <= BOSSVA_BARI_LOWER_5; params++) {
            BossVa* bari = (BossVa*)FindBarinadePart(actor, params);
            if (bari != nullptr && bari->actionFunc != BossVa_BariDeath) {
                BossVa_KillBari(bari, gPlayState);
            }
        }
        *(u8*)BossVa_GetSyncStatic(0) = BOSSVA_PHASE_DEATH;
        BossVa_SetupBodyDeath((BossVa*)actor, gPlayState);
        Enemy_StartFinishingBlow(gPlayState, actor);
        // Its own hit handler fires the defeat hook, not its death
        GameInteractor_ExecuteOnBossDefeat(actor);
        return true;
    },
    .members = { { &sBarinadeTether, FindBarinadePartP<BOSSVA_SUPPORT_1> },
                 { &sBarinadeTether, FindBarinadePartP<BOSSVA_SUPPORT_2> },
                 { &sBarinadeTether, FindBarinadePartP<BOSSVA_SUPPORT_3> },
                 BARINADE_BARI(BOSSVA_BARI_UPPER_1), BARINADE_BARI(BOSSVA_BARI_UPPER_2),
                 BARINADE_BARI(BOSSVA_BARI_UPPER_3), BARINADE_BARI(BOSSVA_BARI_UPPER_4),
                 BARINADE_BARI(BOSSVA_BARI_UPPER_5), BARINADE_BARI(BOSSVA_BARI_LOWER_1),
                 BARINADE_BARI(BOSSVA_BARI_LOWER_2), BARINADE_BARI(BOSSVA_BARI_LOWER_3),
                 BARINADE_BARI(BOSSVA_BARI_LOWER_4), BARINADE_BARI(BOSSVA_BARI_LOWER_5) },
};

// Morpha: the core (in the spawn list) and its one or two tentacles, which keep their state in work[0] (their action
// function never changes). The core's state values and the cutscene states are local to its overlay.
constexpr s16 MO_CORE_STUNNED = 5;
constexpr s16 MO_CORE_ATTACK = 10;
constexpr s16 MO_CORE_RETREAT = 11;
constexpr s16 MO_TENT_ATTACK = 2;
constexpr s16 MO_TENT_GRAB = 4;
constexpr s16 MO_TENT_SHAKE = 5;
constexpr s16 MO_TENT_CUT = 100;
constexpr s16 MO_TENT_DEATH_START = 200;
constexpr s16 MO_DEATH_START = 100;

bool IsLiveBoss(Actor* actor) {
    for (Actor* it = gPlayState->actorCtx.actorLists[ACTORCAT_BOSS].head; it != nullptr; it = it->next) {
        if (it == actor) {
            return it->update != nullptr;
        }
    }
    return false;
}

// The overlay keeps its tentacles in statics, which outlive them
BossMo* MorphaTentacle(s32 which) {
    BossMo* tent = BossMo_GetTentacle(which);
    return tent != nullptr && IsLiveBoss(&tent->actor) ? tent : nullptr;
}

const StateSyncDef sMorphaTentacle = {
    .name = "Morpha's tentacle",
    .actorId = ACTOR_BOSS_MO,
    .size = sizeof(BossMo),
    .match = [](Actor* actor) { return actor->params == BOSSMO_TENTACLE; },
    // Grabbing and shaking hold this game's own Link, so they're its own
    .isLocal = [](Actor* actor) {
        BossMo* tent = (BossMo*)actor;
        s16 state = tent->work[MO_TENT_ACTION_STATE];
        return tent->csCamera != 0 || state == MO_TENT_GRAB || state == MO_TENT_SHAKE;
    },
    .isFighting = [](Actor* actor) {
        BossMo* tent = (BossMo*)actor;
        s16 state = tent->work[MO_TENT_ACTION_STATE];
        return tent->csCamera == 0 && state != MO_TENT_GRAB && state != MO_TENT_SHAKE && state < MO_TENT_DEATH_START;
    },
    .isDefeated = [](Actor* actor) { return ((BossMo*)actor)->work[MO_TENT_ACTION_STATE] >= MO_TENT_DEATH_START; },
    .wasHit = [](Actor* actor, const void* before) {
        return ((BossMo*)actor)->work[MO_TENT_ACTION_STATE] == MO_TENT_CUT &&
               ((const BossMo*)before)->work[MO_TENT_ACTION_STATE] != MO_TENT_CUT;
    },
    // Its shape: the swing it's doing and how far each segment got. Not the segment positions, worked out from those,
    // nor the grab.
    .fields = { FIELD(BossMo, tent2KillTimer), FIELD(BossMo, tentSpawnPos), FIELDS(BossMo, work, timers),
                FIELD(BossMo, fwork), FIELDS(BossMo, baseAlpha, cutScale), FIELDS(BossMo, flattenRate, waterLevelMod),
                FIELDS(BossMo, baseBubblesTimer, drawActor), FIELD(BossMo, targetPos), FIELD(BossMo, tentRippleSize),
                FIELD(BossMo, tentRot), FIELDS(BossMo, tentMaxAngle, tentPulse), FIELD(Actor, scale),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    .isMember = true,
};

const StateSyncDef sMorpha = {
    .name = "Morpha",
    .actorId = ACTOR_BOSS_MO,
    .size = sizeof(BossMo),
    .match = [](Actor* actor) { return actor->params != BOSSMO_TENTACLE; },
    .isFighting = [](Actor* actor) {
        BossMo* core = (BossMo*)actor;
        s16 state = core->work[MO_TENT_ACTION_STATE];
        // Moving, making a tentacle, under water (after a Link in the water), stunned, in the tentacle, retreating
        return core->csState == 0 && (state == 0 || state == 1 || state == 2 || state == MO_CORE_STUNNED ||
                                      state == MO_CORE_ATTACK || state == MO_CORE_RETREAT);
    },
    .isDefeated = [](Actor* actor) { return ((BossMo*)actor)->csState >= MO_DEATH_START; },
    // Hit with a sword it's stunned, with the hookshot while in the tentacle it lets go: either way it's invincible
    // for a moment
    .wasHit = [](Actor* actor, const void* before) {
        return ((BossMo*)actor)->work[MO_CORE_INVINC_TIMER] > ((const BossMo*)before)->work[MO_CORE_INVINC_TIMER];
    },
    // The water level is the core's (plus what each tentacle pulls it by), and sets the water everyone swims in
    .fields = { FIELD(BossMo, hitCount), FIELDS(BossMo, work, timers), FIELD(BossMo, fwork), FIELD(BossMo, baseAlpha),
                FIELD(BossMo, waterLevel), FIELD(BossMo, drawActor), FIELD(BossMo, targetPos),
                FIELDS(BossMo, tentMaxAngle, tentSpeed), FIELD(Actor, scale),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    .defeat = [](Actor* actor) {
        // What its own hit handler does on the killing blow. Not while a tentacle shakes this game's Link.
        BossMo* core = (BossMo*)actor;
        BossMo* tent1 = MorphaTentacle(0);
        BossMo* tent2 = MorphaTentacle(1);
        if ((tent1 != nullptr && tent1->csCamera != 0) || (tent2 != nullptr && tent2->csCamera != 0)) {
            return false;
        }
        actor->colChkInfo.health = 0;
        Enemy_StartFinishingBlow(gPlayState, actor);
        GameInteractor_ExecuteOnBossDefeat(actor);
        Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x100FF);
        core->csState = MO_DEATH_START;
        if (tent1 != nullptr) {
            tent1->drawActor = false;
            tent1->work[MO_TENT_ACTION_STATE] = MO_TENT_DEATH_START;
            tent1->baseAlpha = 0.0f;
        }
        if (tent2 != nullptr) {
            tent2->tent2KillTimer = 1;
        }
        Player* player = GET_PLAYER(gPlayState);
        if (player->actor.parent != nullptr) {
            player->av2.actionVar2 = 0x65;
            player->actor.parent = nullptr;
            player->csAction = 0;
        }
        return true;
    },
    .members = { { &sMorphaTentacle, [](Actor*) { return (Actor*)MorphaTentacle(0); } },
                 { &sMorphaTentacle, [](Actor*) { return (Actor*)MorphaTentacle(1); } } },
    .debug = [](Actor* actor) {
        BossMo* core = (BossMo*)actor;
        return nlohmann::json{ { "state", core->work[MO_TENT_ACTION_STATE] }, { "cs", core->csState },
                               { "invinc", core->work[MO_CORE_INVINC_TIMER] } };
    },
};

// Twinrova: Koume and Kotake fight first, hitting each other with the beams Link reflects (their health counts the
// hits, up), then merge into Twinrova in a cutscene each game plays by itself. Twinrova (in the spawn list) leads both
// halves; while the sisters fight she waits out of sight.
constexpr s16 TW_SISTER_KOTAKE = 0;
constexpr s16 TW_SISTER_KOUME = 1;
constexpr s16 TW_MERGED = 2;

Actor* TwinrovaSister(s16 params) {
    BossTw* sister = BossTw_GetSister(params);
    return sister != nullptr && IsLiveBoss(&sister->actor) ? &sister->actor : nullptr;
}

const StateSyncDef sTwinrovaSister = {
    .name = "Twinrova's sister",
    .actorId = ACTOR_BOSS_TW,
    .size = sizeof(BossTw),
    .match = [](Actor* actor) { return actor->params == TW_SISTER_KOTAKE || actor->params == TW_SISTER_KOUME; },
    .actionOffset = offsetof(BossTw, actionFunc),
    .fightActions = { ACTION(BossTw_FlyTo), ACTION(BossTw_TurnToPlayer), ACTION(BossTw_ShootBeam),
                      ACTION(BossTw_FinishBeamShoot), ACTION(BossTw_HitByBeam), ACTION(BossTw_Laugh),
                      ACTION(BossTw_Spin) },
    .hitReactions = { ACTION(BossTw_HitByBeam) },
    // Each game's beam aims at its own Link, who reflects it with their own shield
    .localActions = { ACTION(BossTw_ShootBeam) },
    .enterLocal = [](Actor* actor) { BossTw_SetupShootBeam((BossTw*)actor, gPlayState); },
    .deathActions = { ACTION(BossTw_DeathCS) },
    .wasHit = [](Actor* actor, const void* before) {
        return actor->colChkInfo.health > ((const BossTw*)before)->actor.colChkInfo.health;
    },
    .skelAnimes = { offsetof(BossTw, skelAnime) },
    .healthFollowsOwner = true,
    .fields = { FIELDS(BossTw, work, workf), FIELD(BossTw, targetPos), FIELD(BossTw, rotateSpeed),
                FIELD(BossTw, scepterAlpha), FIELD(BossTw, visible), FIELD(BossTw, unk_5F8),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    .isMember = true,
};

const StateSyncDef sTwinrova = {
    .name = "Twinrova",
    .actorId = ACTOR_BOSS_TW,
    .size = sizeof(BossTw),
    .match = [](Actor* actor) { return actor->params == TW_MERGED; },
    .actionOffset = offsetof(BossTw, actionFunc),
    .fightActions = { ACTION(BossTw_Wait), ACTION(BossTw_TwinrovaFly), ACTION(BossTw_TwinrovaArriveAtTarget),
                      ACTION(BossTw_TwinrovaChargeBlast), ACTION(BossTw_TwinrovaShootBlast),
                      ACTION(BossTw_TwinrovaDoneBlastShoot), ACTION(BossTw_TwinrovaStun),
                      ACTION(BossTw_TwinrovaGetUp), ACTION(BossTw_TwinrovaSpin), ACTION(BossTw_TwinrovaLaugh) },
    // A charged shield blast stuns her (only then do swords hurt her)
    .hitReactions = { ACTION(BossTw_TwinrovaStun) },
    .deathActions = { ACTION(BossTw_TwinrovaDeathCS) },
    .skelAnimes = { offsetof(BossTw, skelAnime) },
    .fields = { FIELDS(BossTw, work, workf), FIELD(BossTw, csState1), FIELD(BossTw, targetPos),
                FIELD(BossTw, rotateSpeed), FIELD(BossTw, visible), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    // Which blast she throws: each game spawns its own (aimed at its own Link), of the same kind
    .statics = { { [] { return (void*)BossTw_GetBlastState(0); }, 1 },
                 { [] { return (void*)BossTw_GetBlastState(1); }, 1 },
                 { [] { return (void*)BossTw_GetBlastState(2); }, 1 } },
    // Never into or out of the sisters' half: the merge between them is each game's own cutscene
    .onApply = [](Actor* actor, const void* before) {
        if ((((const BossTw*)before)->actionFunc == BossTw_Wait) != (((BossTw*)actor)->actionFunc == BossTw_Wait)) {
            memcpy((void*)actor, before, sizeof(BossTw));
        }
    },
    .defeat = [](Actor* actor) {
        if (((BossTw*)actor)->actionFunc == BossTw_Wait) {
            return false; // Not merged here yet
        }
        actor->colChkInfo.health = 0;
        BossTw_TwinrovaSetupDeathCS((BossTw*)actor, gPlayState);
        Enemy_StartFinishingBlow(gPlayState, actor);
        Audio_PlayActorSound2(actor, NA_SE_EN_TWINROBA_YOUNG_DEAD);
        GameInteractor_ExecuteOnBossDefeat(actor);
        return true;
    },
    .members = { { &sTwinrovaSister, [](Actor*) { return TwinrovaSister(TW_SISTER_KOTAKE); } },
                 { &sTwinrovaSister, [](Actor*) { return TwinrovaSister(TW_SISTER_KOUME); } } },
};

// Ganondorf. Each game rallies its own light ball with its own Link: a ball that gets back to him on another game
// knocks him back there, which goes to the room owner like any hit.
const StateSyncDef sGanondorf = {
    .name = "Ganondorf",
    .actorId = ACTOR_BOSS_GANON,
    .size = sizeof(BossGanon),
    // Not his light balls and lightning (0x64 and up), nor the one at the top of the collapsing tower (1)
    .match = [](Actor* actor) { return actor->params < 0x64 && actor->params != 1; },
    .actionOffset = offsetof(BossGanon, actionFunc),
    .fightActions = { ACTION(BossGanon_Wait), ACTION(BossGanon_ChargeLightBall), ACTION(BossGanon_PlayTennis),
                      ACTION(BossGanon_PoundFloor), ACTION(BossGanon_ChargeBigMagic), ACTION(BossGanon_Block),
                      ACTION(BossGanon_HitByLightBall), ACTION(BossGanon_Vulnerable), ACTION(BossGanon_Damaged) },
    .hitReactions = { ACTION(BossGanon_HitByLightBall), ACTION(BossGanon_Vulnerable), ACTION(BossGanon_Damaged) },
    .deathActions = { ACTION(BossGanon_DeathAndTowerCutscene) },
    .skelAnimes = { offsetof(BossGanon, skelAnime) },
    // Not the floor tiles' collapse (a random pattern each game runs by itself), the shock on the local Link, nor the
    // cutscenes
    .fields = { FIELD(BossGanon, legSwayEnabled), FIELD(BossGanon, envLightMode),
                FIELDS(BossGanon, unk_1A2, triforceType), FIELD(BossGanon, timers),
                FIELDS(BossGanon, unk_1C2, screenFlashTimer), FIELD(BossGanon, fwork),
                FIELDS(BossGanon, handLightBallScale, unk_258), FIELDS(BossGanon, unk_26C, unk_270),
                FIELDS(BossGanon, unk_278, unk_290), FIELD(BossGanon, unk_2D0), FIELD(BossGanon, unk_2D4),
                FIELD(BossGanon, shockGlow), FIELD(BossGanon, unk_508), FIELD(BossGanon, lensFlareMode),
                FIELD(BossGanon, lensFlareTimer), FIELD(BossGanon, lensFlareScale), FIELD(BossGanon, useOpenHand),
                FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    // Only the room owner's pound drops floor tiles (a random pattern), which then fall on everyone's game
    .onApply = [](Actor* actor, const void* before) { ((BossGanon*)actor)->unk_19E = 4; },
    .defeat = [](Actor* actor) {
        // What his own hit handler does on the killing blow. His death needs its animation object, which may still be
        // loading.
        BossGanon* gdf = (BossGanon*)actor;
        BossGanon_SetupDeathCutscene(gdf, gPlayState);
        if (gdf->actionFunc != BossGanon_DeathAndTowerCutscene) {
            return false;
        }
        actor->colChkInfo.health = 0;
        Audio_PlayActorSound2(actor, NA_SE_EN_GANON_DEAD);
        Audio_PlayActorSound2(actor, NA_SE_EN_GANON_DD_THUNDER);
        Audio_QueueSeqCmd(0x100100FF);
        gdf->screenFlashTimer = 4;
        GameInteractor_ExecuteOnBossDefeat(actor);
        return true;
    },
};

// Ganon. Knocked down, he waits for each player to pick up their own Master Sword (each game plays that part by
// itself, from when the room owner's goes down). He goes after the room owner's Link; which way he faces is each
// game's own, since that decides whether a hit lands on his tail or his face.
const StateSyncDef sGanon = {
    .name = "Ganon",
    .actorId = ACTOR_BOSS_GANON2,
    .size = sizeof(BossGanon2),
    .actionOffset = offsetof(BossGanon2, actionFunc),
    .fightActions = { ACTION(func_808FFDB0), ACTION(func_808FFEBC), ACTION(func_808FFFE0), ACTION(func_80900104),
                      ACTION(func_8090026C), ACTION(func_80900344), ACTION(func_80900650), ACTION(func_80900890) },
    .hitReactions = { ACTION(func_808FFFE0), ACTION(func_80900104), ACTION(func_8090026C), ACTION(func_80900890) },
    .localActions = { ACTION(func_80900890) },
    .enterLocal = [](Actor* actor) { func_80900818((BossGanon2*)actor, gPlayState); },
    .deathActions = { ACTION(func_8090120C) },
    .skelAnimes = { offsetof(BossGanon2, skelAnime) },
    // Not whether this game's Link has the sword back, nor which of his parts faces this game's Link
    .fields = { FIELDS(BossGanon2, unk_194, unk_19C), FIELD(BossGanon2, unk_1A2), FIELD(BossGanon2, unk_1AC),
                FIELDS(BossGanon2, unk_224, unk_228), FIELD(BossGanon2, unk_30C), FIELD(BossGanon2, unk_311),
                FIELD(BossGanon2, unk_316), FIELDS(BossGanon2, unk_31A, unk_324), FIELD(BossGanon2, unk_336),
                FIELD(BossGanon2, unk_338), FIELDS(BossGanon2, unk_342, unk_346), FIELDS(BossGanon2, unk_35C, unk_370),
                FIELDS(BossGanon2, unk_390, unk_392), FLAGS(Actor, flags, ACTOR_FLAG_ATTENTION_ENABLED) },
    .defeat = [](Actor* actor) {
        // He falls, and each game's Link finishes him with their own sword (which fires the defeat hook)
        actor->colChkInfo.health = 0;
        func_80901020((BossGanon2*)actor, gPlayState);
        return true;
    },
    .targetsOwnLink = true,
    .debug = [](Actor* actor) {
        BossGanon2* ganon = (BossGanon2*)actor;
        Player* player = GET_PLAYER(gPlayState);
        return nlohmann::json{ { "cs", ganon->csState },
                               { "csTimer", ganon->csTimer },
                               { "linkState1", player->stateFlags1 },
                               { "linkCsAction", player->csAction },
                               { "freeze", actor->freezeTimer } };
    },
};

// MARK: - People and animals

/**
 * Some townspeople and animals walk paths or wander about at random. The room owner sends where they are and what
 * they're doing (a few times a second; they walk on by themselves in between), so everyone sees them in the same
 * place. Only while they walk, wait or wander: talking to one, picking one up and the cutscenes they're in are each
 * game's own (see StateSyncDef::npc).
 */
const StateSyncDef sMarketWalker = {
    .name = "Townsperson",
    .actorId = ACTOR_EN_HY,
    .size = sizeof(EnHy),
    .actionOffset = offsetof(EnHy, actionFunc),
    // Walking a loop, or pacing up and down once a Link has come near
    .fightActions = { ACTION(EnHy_Walk), ACTION(EnHy_Pace) },
    .isFighting = [](Actor* actor) {
        return ActionIndex(actor, sMarketWalker) >= 0 && ((EnHy*)actor)->interactInfo.talkState == 0;
    },
    .skelAnimes = { offsetof(EnHy, skelAnime) },
    .fields = { FIELD(EnHy, pathReverse), FIELD(EnHy, waypoint) },
    .npc = true,
    .sendEvery = 3,
};

const StateSyncDef sCarpenter = {
    .name = "Carpenter",
    .actorId = ACTOR_EN_DAIKU_KAKARIKO,
    .size = sizeof(EnDaikuKakariko),
    .actionOffset = offsetof(EnDaikuKakariko, actionFunc),
    .fightActions = { ACTION(EnDaikuKakariko_Wait), ACTION(EnDaikuKakariko_Run),
                      ACTION(EnDaikuKakariko_StopRunning) },
    .isFighting = [](Actor* actor) {
        return ActionIndex(actor, sCarpenter) >= 0 && ((EnDaikuKakariko*)actor)->talkState == 0;
    },
    .skelAnimes = { offsetof(EnDaikuKakariko, skelAnime) },
    .fields = { FIELDS(EnDaikuKakariko, waypoint, currentAnimIndex), FIELDS(EnDaikuKakariko, pathContinue, run),
                FIELD(EnDaikuKakariko, timer) },
    .npc = true,
    .sendEvery = 3,
};

const StateSyncDef sGraveyardKid = {
    .name = "Graveyard kid",
    .actorId = ACTOR_EN_CS,
    .size = sizeof(EnCs),
    .actionOffset = offsetof(EnCs, actionFunc),
    .fightActions = { ACTION(EnCs_Walk), ACTION(EnCs_Wait) },
    .isFighting = [](Actor* actor) { return ActionIndex(actor, sGraveyardKid) >= 0 && ((EnCs*)actor)->talkState == 0; },
    .skelAnimes = { offsetof(EnCs, skelAnime) },
    .fields = { FIELDS(EnCs, waypoint, walkSpeed), FIELD(EnCs, currentAnimIndex) },
    .npc = true,
    .sendEvery = 3,
};

const StateSyncDef sRunningMan = {
    .name = "Running Man",
    .actorId = ACTOR_EN_MM,
    .size = sizeof(EnMm),
    .actionOffset = offsetof(EnMm, actionFunc),
    // Running his path, sitting down and getting up, waiting (not talking about masks)
    .fightActions = { ACTION(func_80AAE294), ACTION(func_80AAE224), ACTION(func_80AAE50C), ACTION(func_80AAE598) },
    .skelAnimes = { offsetof(EnMm, skelAnime) },
    .fields = { FIELDS(EnMm, waypoint, speedXZ), FIELD(EnMm, curAnimIndex), FIELD(EnMm, sitTimer) },
    .npc = true,
    .sendEvery = 3,
};

const StateSyncDef sDog = {
    .name = "Dog",
    .actorId = ACTOR_EN_DOG,
    .size = sizeof(EnDog),
    .actionOffset = offsetof(EnDog, actionFunc),
    // Not one following a Link (who's carrying it to its owner, on their own game)
    .fightActions = { ACTION(EnDog_FollowPath), ACTION(EnDog_ChooseMovement), ACTION(EnDog_Wait) },
    .skelAnimes = { offsetof(EnDog, skelAnime) },
    .fields = { FIELD(EnDog, reverse), FIELD(EnDog, waypoint), FIELD(EnDog, behaviorTimer),
                FIELDS(EnDog, nextBehavior, behavior) },
    .npc = true,
    .sendEvery = 3,
};

// The horses wandering Lon Lon Ranch (not the ones standing in their stalls, which stay put). They keep what they're
// doing in a number, not an action function.
constexpr s32 HORSE_NORMAL_WAIT_CLONE = 3;

const StateSyncDef sRanchHorse = {
    .name = "Horse",
    .actorId = ACTOR_EN_HORSE_NORMAL,
    .size = sizeof(EnHorseNormal),
    .isFighting = [](Actor* actor) { return ((EnHorseNormal*)actor)->action != HORSE_NORMAL_WAIT_CLONE; },
    .skelAnimes = { offsetof(EnHorseNormal, skin.skelAnime) },
    .fields = { FIELDS(EnHorseNormal, action, animationIdx), FIELD(EnHorseNormal, unk_1E4),
                FIELDS(EnHorseNormal, unk_218, unk_220), FIELD(EnHorseNormal, waypoint) },
    .npc = true,
    .sendEvery = 1, // They gallop: every frame, or they drift apart between states
};
// clang-format on

const std::vector<const StateSyncDef*>& StateSyncDefs() {
    static const std::vector<const StateSyncDef*> defs = {
        &sGohma,
        &sKingDodongo,
        &sPhantomGanon,
        &sPhantomGanonHorse,
        &sVolvagia,
        &sVolvagiaHead,
        &sBongoBongo,
        &sBongoBongoHand,
        &sBarinade,
        &sBarinadeTether,
        &sBarinadeBari,
        &sMorpha,
        &sMorphaTentacle,
        &sTwinrova,
        &sTwinrovaSister,
        &sGanondorf,
        &sGanon,
        &sMarketWalker,
        &sCarpenter,
        &sGraveyardKid,
        &sRunningMan,
        &sDog,
        &sRanchHorse,
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
    sBreakableKeys.clear();
    sBreakableWasCut.clear();
    sGone.clear();
    sTakenHere.clear();
    sSeenActors.clear();
}

void CheckNewActor(Actor* actor, Key key);

void OnShouldActorUpdate(Actor* actor, bool* should) {
    const StateSyncDef* def = FindDef(actor);
    bool isEnemy = def == nullptr && IsSyncedEnemy(actor);
    if ((!isEnemy && def == nullptr && actor->id != ACTOR_OBJ_OSHIHIKI && !IsBreakable(actor)) || !IsEnabled()) {
        return;
    }
    CheckScene();
    DeriveKey(actor);
    Key key;
    if (!HasPeers()) {
        return;
    }
    if (IsBreakable(actor)) {
        if (GetKey(actor, &key)) {
            CheckNewActor(actor, key);
            BreakablePreUpdate(actor, key, should);
        }
    } else if (def != nullptr) {
        if (!def->targetsOwnLink && !def->npc) {
            TargetNearestLink(actor);
        }
        if (!def->isMember && GetKey(actor, &key)) {
            CheckNewActor(actor, key);
            SyncedPreUpdate(actor, key, *def);
        }
    } else if (GetKey(actor, &key)) {
        CheckNewActor(actor, key);
        if (isEnemy) {
            EnemyPreUpdate(actor, key, should);
        } else {
            BlockPreUpdate(actor, key);
        }
    }
}

void OnActorUpdate(Actor* actor) {
    if (IsBreakable(actor)) {
        if (IsEnabled() && HasPeers()) {
            BreakablePostUpdate(actor);
        }
        return;
    }
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
// What's known about the actor that had this key before (a room that loads again spawns new copies of its actors)
void ForgetKey(Key key) {
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

// An actor from a spawn list gets its key after its init (which runs while it's spawned), so a new one is noticed at
// its first update instead
void CheckNewActor(Actor* actor, Key key) {
    if (sSeenActors.emplace(actor).second) {
        ForgetKey(key);
    }
}

void OnActorInit(Actor* actor) {
    if (gPlayState == nullptr) {
        return;
    }
    CheckScene();
    Key key;
    if (!IsEnabled() || !GetKey(actor, &key)) {
        return;
    }
    sSeenActors.insert(actor);
    ForgetKey(key);
}

void OnActorDestroy(Actor* actor) {
    if (sDebugBusy == actor) {
        sDebugBusy = nullptr;
    }
    sSeenActors.erase(actor);
    sDerivedKeys.erase(actor);
    sBreakableKeys.erase(actor);
    sBreakableWasCut.erase(actor);
    sTakenHere.erase(actor);
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
        ForgetGoneInEmptyRooms();
        ForgetSyncedInEmptyRooms();
    }
    if (!HasPeers()) {
        return;
    }
    nlohmann::json states = nlohmann::json::array();
    for (auto& [key, synced] : sSynced) {
        CheckSynced(key, synced);
        AddSyncedState(key, synced, states);
    }
    if (!states.empty()) {
        nlohmann::json payload;
        payload["type"] = BOSS_STATE;
        payload["states"] = std::move(states);
        SendToPeers(payload);
    }
    FollowRoomOwnerOffscreen();
    SendEnemyStates();
    if (sFrame % SLOW_TICK == 0) {
        SendBlockPositions();
        SendDeadList();
        SendDefeated();
        SendGoneList();
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

// One fight's state from a BOSS_STATE
static void HandleSyncedState(nlohmann::json& payload, uint32_t sender) {
    Key key;
    Synced* synced = FindSynced(payload, &key);
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

void HandlePacket_BossState(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    uint32_t sender = payload.value("clientId", (uint32_t)0);
    for (auto& state : payload["states"]) {
        state["sceneNum"] = payload["sceneNum"];
        HandleSyncedState(state, sender);
    }
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

void HandlePacket_PropBreak(nlohmann::json& payload) {
    if (!IsForThisScene(payload)) {
        return;
    }
    CheckScene();
    bool cut = payload.value("cut", false);
    bool effect = payload.value("effect", false);
    Player* player = GET_PLAYER(gPlayState);
    for (auto& entry : payload["props"]) {
        Key key = entry.get<Key>();
        if (!cut) {
            sGone.insert(key);
        }
        Actor* actor = FindActor(key);
        if (actor == nullptr || actor->update == nullptr || !IsBreakable(actor) || sTakenHere.contains(actor) ||
            player->heldActor == actor || (cut && IsCut(actor))) {
            continue;
        }
        BreakHere(actor, cut, effect);
        if (cut) {
            sBreakableWasCut[actor] = true;
        }
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
    { "Barinade", 0, offsetof(BossVa, colliderBody) },
    { "Barinade", 1, offsetof(BossVa, colliderSph) },
    { "Barinade", 2, offsetof(BossVa, colliderSph) },
    { "Barinade", 3, offsetof(BossVa, colliderSph) },
    { "Barinade", 4, offsetof(BossVa, colliderSph) },
    { "Morpha", 0, offsetof(BossMo, coreCollider) },
    { "Morpha", 1, offsetof(BossMo, tentCollider) },
    { "Morpha", 2, offsetof(BossMo, tentCollider) },
    { "Twinrova", 0, offsetof(BossTw, collider) },
    { "Ganondorf", 0, offsetof(BossGanon, collider) },
    { "Ganon", 0, offsetof(BossGanon2, unk_424) },
};

// Things that happen to a fight only on the game of the player who did them, other than collider hits
struct DebugPoke {
    const char* name;
    int member;
    bool (*poke)(Actor* actor); // false if it can't happen right now
};

// What the other sister's beam does when it's reflected into her (BossTw_ShootBeam)
bool DebugTwinrovaBeamHit(Actor* actor) {
    BossTw* sister = (BossTw*)actor;
    if (sister->actionFunc != BossTw_FlyTo && sister->actionFunc != BossTw_TurnToPlayer &&
        sister->actionFunc != BossTw_Laugh && sister->actionFunc != BossTw_FinishBeamShoot) {
        return false;
    }
    BossTw_SetupHitByBeam(sister, gPlayState);
    actor->colChkInfo.health++;
    return true;
}

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
    // A beam this game's Link reflected hits Kotake (or Koume)
    { "Twinrova", 1, [](Actor* actor) { return DebugTwinrovaBeamHit(actor); } },
    { "Twinrova", 2, [](Actor* actor) { return DebugTwinrovaBeamHit(actor); } },
    // A charged shield blast reaches Twinrova
    { "Twinrova", 0,
      [](Actor* actor) {
          BossTw* tw = (BossTw*)actor;
          if (tw->actionFunc == BossTw_Wait || tw->actionFunc == BossTw_TwinrovaStun) {
              return false;
          }
          tw->twinrovaStun = 1;
          return true;
      } },
    // This game's light ball gets back to him
    { "Ganondorf", 0,
      [](Actor* actor) {
          BossGanon* gdf = (BossGanon*)actor;
          if (gdf->actionFunc != BossGanon_Wait && gdf->actionFunc != BossGanon_PlayTennis &&
              gdf->actionFunc != BossGanon_ChargeLightBall) {
              return false;
          }
          BossGanon_SetupHitByLightBall(gdf, gPlayState);
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
             { "frame", def.skelAnimes.empty() ? 0.0f : ((SkelAnime*)((u8*)actor + def.skelAnimes[0]))->curFrame },
             { "more", def.debug != nullptr ? def.debug(actor) : nlohmann::json() } };
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
                    (category == ACTORCAT_PROP && actor->id != ACTOR_OBJ_OSHIHIKI && !IsBreakable(actor))) {
                    continue;
                }
                s16 index = (s16)(key & 0xFFFF);
                // Frames since the room owner's last state for it arrived, and since this game last sent one
                auto remote = sRemote.find(key);
                auto sent = sSent.find(key);
                int remoteAge = remote == sRemote.end() ? -1 : (int)(sFrame - remote->second.frame);
                int sentAge = sent == sSent.end() ? -1 : (int)(sFrame - sent->second.frame);
                list.push_back({ { "room", actor->room },
                                 { "index", index },
                                 { "id", actor->id },
                                 { "x", actor->world.pos.x },
                                 { "y", actor->world.pos.y },
                                 { "z", actor->world.pos.z },
                                 { "rotY", actor->shape.rot.y },
                                 { "health", actor->colChkInfo.health },
                                 { "alive", actor->update != nullptr },
                                 { "cut", IsBreakable(actor) && IsCut(actor) },
                                 { "remoteAge", remoteAge },
                                 { "sentAge", sentAge },
                                 { "dead", sDead.contains(key) } });
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
                entry["ptr"] = (uintptr_t)actor;
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
                                      { "taken", taken },
                                      { "updates", synced->second.updates },
                                      { "ptrs", [&] {
                                           nlohmann::json ptrs = nlohmann::json::array();
                                           for (Actor* a : synced->second.actors) {
                                               ptrs.push_back((uintptr_t)a);
                                           }
                                           return ptrs;
                                       }() } };
                }
                nlohmann::json members = nlohmann::json::array();
                for (const GroupMember& member : def->members) {
                    Actor* found = member.find(actor);
                    nlohmann::json state = nullptr;
                    if (found != nullptr && found->update != nullptr) {
                        state = DebugState(found, *member.def);
                        state["ptr"] = (uintptr_t)found;
                    }
                    members.push_back(state);
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

// Whether a textbox is open (boss intros wait for theirs to be read)
EMSCRIPTEN_KEEPALIVE int web_debug_textbox_open(void) {
    return gPlayState != nullptr && Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_NONE;
}

// Close the textbox that's open, if any. Returns whether one was open.
EMSCRIPTEN_KEEPALIVE int web_debug_close_textbox(void) {
    if (gPlayState == nullptr || Message_GetState(&gPlayState->msgCtx) == TEXT_STATE_NONE) {
        return 0;
    }
    Message_CloseTextbox(gPlayState);
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

// Act as if this game's Link were talking to an actor (on: 1) or not (0), for the people and animals that stop
// following the room owner's state then
EMSCRIPTEN_KEEPALIVE int web_debug_npc_busy(int room, int index, int on) {
    Actor* actor = WebDebug_FindActor(room, index);
    if (actor == nullptr || actor->update == nullptr) {
        return 0;
    }
    sDebugBusy = on ? actor : nullptr;
    return 1;
}

// Hit a pot, bush or rock on the next frame, as if with an attack with these damage flags (0x200: a Master Sword
// slash, 0x40: the hammer); or drop one of the floor tiles in Ganondorf's room
EMSCRIPTEN_KEEPALIVE int web_debug_prop_hit(int room, int index, int dmgFlags) {
    static ColliderInfo attack;
    static Collider attacker;
    Actor* actor = WebDebug_FindActor(room, index);
    if (actor == nullptr || actor->update == nullptr || !IsBreakable(actor)) {
        return 0;
    }
    if (actor->id == ACTOR_BG_GANON_OTYUKA) {
        // A floor tile in Ganondorf's room: dropped, like his pound drops it
        ((BgGanonOtyuka*)actor)->isFalling = true;
        return 1;
    }
    ColliderCylinder* collider = actor->id == ACTOR_OBJ_TSUBO  ? &((ObjTsubo*)actor)->collider
                                 : actor->id == ACTOR_EN_KUSA ? &((EnKusa*)actor)->collider
                                                              : &((EnIshi*)actor)->collider;
    attacker.actor = &GET_PLAYER(gPlayState)->actor;
    attack.toucher.dmgFlags = dmgFlags;
    attack.toucher.damage = 2;
    collider->base.acFlags |= AC_HIT;
    collider->base.ac = attacker.actor;
    collider->info.bumperFlags |= BUMP_HIT;
    collider->info.acHit = &attacker;
    collider->info.acHitInfo = &attack;
    return 1;
}
}
#endif
