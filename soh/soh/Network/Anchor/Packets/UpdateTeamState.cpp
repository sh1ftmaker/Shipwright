#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include <nlohmann/json.hpp>
#include "soh/OTRGlobals.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/randomizer/randomizer.h"

extern "C" {
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

/**
 * UPDATE_TEAM_STATE
 *
 * Pushes the current save state to the server for other teammates to use.
 *
 * Fires when the server passes on a REQUEST_TEAM_STATE packet, or when this client saves the game
 *
 * When sending this packet we will assume that the team queue has been emptied for this client, so the queue
 * stored in the server will be cleared.
 *
 * When receiving this packet, if there is items in the team queue, we will play them back in order.
 */

// Our save as the team state
static json PrepTeamState() {
    json state;
    state = gSaveContext;
    // manually update current scene flags
    state["sceneFlags"][gPlayState->sceneNum * 4] = gPlayState->actorCtx.flags.chest;
    state["sceneFlags"][gPlayState->sceneNum * 4 + 1] = gPlayState->actorCtx.flags.swch;
    state["sceneFlags"][gPlayState->sceneNum * 4 + 2] = gPlayState->actorCtx.flags.clear;
    state["sceneFlags"][gPlayState->sceneNum * 4 + 3] = gPlayState->actorCtx.flags.collect;

    // The commented out code below is an attempt at sending the entire randomizer seed over, in hopes that a player
    // doesn't have to generate the seed themselves Currently it doesn't work :)
    if (IS_RANDO) {
        auto randoContext = Rando::Context::GetInstance();

        state["rando"] = json::object();
        state["rando"]["itemLocations"] = json::array();
        for (int i = 0; i < RC_MAX; i++) {
            state["rando"]["itemLocations"][i] = json::array();
            // state["rando"]["itemLocations"][i]["rgID"] =
            // randoContext->GetItemLocation(i)->GetPlacedRandomizerGet();
            state["rando"]["itemLocations"][i][0] = randoContext->GetItemLocation(i)->GetCheckStatus();
            state["rando"]["itemLocations"][i][1] = (u8)randoContext->GetItemLocation(i)->GetIsSkipped();

            // if (randoContext->GetItemLocation(i)->GetPlacedRandomizerGet() == RG_ICE_TRAP) {
            //     state["rando"]["itemLocations"][i]["fakeRgID"] =
            //     randoContext->GetItemOverride(i).LooksLike();
            //     state["rando"]["itemLocations"][i]["trickName"] = json::object();
            //     state["rando"]["itemLocations"][i]["trickName"]["english"] =
            //     randoContext->GetItemOverride(i).GetTrickName().GetEnglish();
            //     state["rando"]["itemLocations"][i]["trickName"]["french"] =
            //     randoContext->GetItemOverride(i).GetTrickName().GetFrench();
            // }
            // if (randoContext->GetItemLocation(i)->HasCustomPrice()) {
            //     state["rando"]["itemLocations"][i]["price"] =
            //     randoContext->GetItemLocation(i)->GetPrice();
            // }
        }

        // auto entranceCtx = randoContext->GetEntranceShuffler();
        // for (int i = 0; i < ENTRANCE_OVERRIDES_MAX_COUNT; i++) {
        //     state["rando"]["entrances"][i] = json::object();
        //     state["rando"]["entrances"][i]["type"] = entranceCtx->entranceOverrides[i].type;
        //     state["rando"]["entrances"][i]["index"] = entranceCtx->entranceOverrides[i].index;
        //     state["rando"]["entrances"][i]["destination"] = entranceCtx->entranceOverrides[i].destination;
        //     state["rando"]["entrances"][i]["override"] = entranceCtx->entranceOverrides[i].override;
        //     state["rando"]["entrances"][i]["overrideDestination"] =
        //     entranceCtx->entranceOverrides[i].overrideDestination;
        // }

        // state["rando"]["seed"] = json::array();
        // for (int i = 0; i < randoContext->hashIconIndexes.size(); i++) {
        //     state["rando"]["seed"][i] = randoContext->hashIconIndexes[i];
        // }
        // state["rando"]["inputSeed"] = randoContext->GetSeedString();
        // state["rando"]["finalSeed"] = randoContext->GetSeed();

        // state["rando"]["randoSettings"] = json::array();
        // for (int i = 0; i < RSK_MAX; i++) {
        //     state["rando"]["randoSettings"][i] =
        //     randoContext->GetOption((RandomizerSettingKey(i))).GetSelectedOptionIndex();
        // }

        // state["rando"]["masterQuestDungeonCount"] = randoContext->GetDungeons()->CountMQ();
        // state["rando"]["masterQuestDungeons"] = json::array();
        // for (int i = 0; i < randoContext->GetDungeons()->GetDungeonListSize(); i++) {
        //     state["rando"]["masterQuestDungeons"][i] = randoContext->GetDungeon(i)->IsMQ();
        // }
        // for (int i = 0; i < randoContext->GetTrials()->GetTrialListSize(); i++) {
        //     state["rando"]["requiredTrials"][i] = randoContext->GetTrial(i)->IsRequired();
        // }
    }

    return state;
}

void Anchor::SendPacket_UpdateTeamState() {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

    // Assume the team queue has been emptied, so clear it
    payload["queue"] = json::array();

    payload["state"] = PrepTeamState();

    SendJsonToRemote(payload);
}

void Anchor::SendPacket_ClearTeamState(std::string teamId) {
    json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = teamId;
    payload["queue"] = json::array();
    payload["state"] = json::object();
    SendJsonToRemote(payload);
}

/**
 * MERGE_TEAM_STATE
 *
 * Like UPDATE_TEAM_STATE, but sent to everyone on the team (the server only forwards UPDATE_TEAM_STATE to the
 * players who asked for it). Sent after merging a team state that lacked some of our progress, so teammates who are
 * already playing get it too. Receivers merge it the same way and answer only if they in turn have something it
 * lacks, so this settles after a round or two.
 */

void Anchor::SendPacket_MergeTeamState() {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    json payload;
    payload["type"] = MERGE_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["state"] = PrepTeamState();
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_MergeTeamState(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !payload.contains("state")) {
        return;
    }

    isHandlingUpdateTeamState = true;
    if (MergeTeamState(payload["state"])) {
        pendingTeamStatePush = true;
    }
    isHandlingUpdateTeamState = false;
}

// OR the bits of theirs into mine, except the ones in localMask
template <typename T> static void MergeBits(T& mine, T theirs, T localMask, bool& ownExtra, bool& gained) {
    T shared = (T)~localMask;
    ownExtra |= ((mine & ~theirs) & shared) != 0;
    gained |= ((theirs & ~mine) & shared) != 0;
    mine = (T)(mine | (theirs & shared));
}

template <typename T> static void MergeMax(T& mine, T theirs, bool& ownExtra, bool& gained) {
    ownExtra |= mine > theirs;
    gained |= theirs > mine;
    mine = MAX(mine, theirs);
}

static bool IsBottleContent(u8 item) {
    return item >= ITEM_POTION_RED && item <= ITEM_POE && item != ITEM_LETTER_RUTO;
}

/**
 * Merges a teammate's (or the server's last) save state into ours. Nothing we have is taken away: flags, quest items,
 * songs, equipment and dungeon items are OR'd, capacities and counts take the bigger value, and items we have that
 * the state lacks are kept. Per-player flags (WorldSync.h), bottle contents and ammo stay ours.
 *
 * Small keys can't be OR'd. A key count only goes down by opening a door, which sets a flag in that dungeon, so we
 * compare who has dungeon flags the other lacks: if only we do, our count is the newer one; if only they do, theirs
 * is; otherwise (same flags, or both played the dungeon on their own) the bigger count, so nobody loses a key they
 * may need. A count therefore only drops when the other side has opened something we haven't.
 *
 * Returns true if we have progress the state lacks, so teammates should get ours.
 */
bool Anchor::MergeTeamState(nlohmann::json& state) {
    // Only from a file of the same kind: a Boss Rush file's flags and items would wreck a normal file, and the other
    // way round
    s32 theirQuest = state.contains("ship") && state["ship"].contains("quest")
                         ? state["ship"]["quest"].value("id", (s32)-1)
                         : (s32)-1;
    if (theirQuest >= 0 && QuestKind(theirQuest) != QuestKind(gSaveContext.ship.quest.id)) {
        questMismatches++;
        SPDLOG_WARN("[Anchor] The team's save is another kind of file (quest {}, ours {}): not merged", theirQuest,
                    gSaveContext.ship.quest.id);
        Notification::Emit({
            .message = theirQuest == QUEST_BOSSRUSH || IS_BOSS_RUSH
                           ? "The team's save and yours aren't both Boss Rush: not merged"
                           : "The team's save is another kind of file than yours: not merged",
            .remainingTime = 10.0f,
        });
        return false;
    }

    SaveContext loadedData = state.get<SaveContext>();
    bool loaded = IsSaveLoaded();
    bool ownExtra = false;
    bool gained = false;

    // Scene flags. The current scene's live in the actor context until the scene is left
    bool dungeonOwnExtra[ARRAY_COUNT(gSaveContext.inventory.dungeonKeys)] = {};
    bool dungeonGained[ARRAY_COUNT(gSaveContext.inventory.dungeonKeys)] = {};
    for (s16 i = 0; i < ARRAY_COUNT(gSaveContext.sceneFlags); i++) {
        bool current = loaded && gPlayState->sceneNum == i;
        u32* mine[4] = { &gSaveContext.sceneFlags[i].chest, &gSaveContext.sceneFlags[i].swch,
                         &gSaveContext.sceneFlags[i].clear, &gSaveContext.sceneFlags[i].collect };
        if (current) {
            mine[0] = &gPlayState->actorCtx.flags.chest;
            mine[1] = &gPlayState->actorCtx.flags.swch;
            mine[2] = &gPlayState->actorCtx.flags.clear;
            mine[3] = &gPlayState->actorCtx.flags.collect;
        }
        u32 theirs[4] = { loadedData.sceneFlags[i].chest, loadedData.sceneFlags[i].swch,
                          loadedData.sceneFlags[i].clear, loadedData.sceneFlags[i].collect };
        s16 types[4] = { FLAG_SCENE_TREASURE, FLAG_SCENE_SWITCH, FLAG_SCENE_CLEAR, FLAG_SCENE_COLLECTIBLE };

        bool sceneOwnExtra = false;
        bool sceneGained = false;
        for (int t = 0; t < 4; t++) {
            u32 localMask = 0;
            for (s16 bit = 0; bit < 32; bit++) {
                if (AnchorWorldSync::IsPerPlayerFlag(i, types[t], bit)) {
                    localMask |= 1u << bit;
                }
            }
            u32 newBits = theirs[t] & ~*mine[t] & ~localMask;
            for (s16 bit = 0; newBits != 0 && bit < 32; bit++) {
                if (newBits & (1u << bit)) {
                    AnchorWorldSync::MarkSetByTeammate({ i, types[t], bit });
                }
            }
            MergeBits(*mine[t], theirs[t], localMask, sceneOwnExtra, sceneGained);
        }
        if (current) {
            gSaveContext.sceneFlags[i].chest = *mine[0];
            gSaveContext.sceneFlags[i].swch = *mine[1];
            gSaveContext.sceneFlags[i].clear = *mine[2];
            gSaveContext.sceneFlags[i].collect = *mine[3];
        }
        if (i < ARRAY_COUNT(dungeonOwnExtra)) {
            dungeonOwnExtra[i] = sceneOwnExtra;
            dungeonGained[i] = sceneGained;
        }
        ownExtra |= sceneOwnExtra;
        gained |= sceneGained;
    }

    for (int i = 0; i < ARRAY_COUNT(gSaveContext.eventChkInf); i++) {
        MergeBits(gSaveContext.eventChkInf[i], loadedData.eventChkInf[i], (u16)0, ownExtra, gained);
    }
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.itemGetInf); i++) {
        MergeBits(gSaveContext.itemGetInf[i], loadedData.itemGetInf[i], (u16)0, ownExtra, gained);
    }
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.infTable); i++) {
        u16 localMask = 0;
        for (s16 bit = 0; bit < 16; bit++) {
            if (AnchorWorldSync::IsPerPlayerFlag(SCENE_ID_MAX, FLAG_INF_TABLE, i * 16 + bit)) {
                localMask |= 1 << bit;
            }
        }
        MergeBits(gSaveContext.infTable[i], loadedData.infTable[i], localMask, ownExtra, gained);
    }
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.ship.randomizerInf); i++) {
        MergeBits(gSaveContext.ship.randomizerInf[i], loadedData.ship.randomizerInf[i], (u16)0, ownExtra, gained);
    }
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.gsFlags); i++) {
        MergeBits(gSaveContext.gsFlags[i], loadedData.gsFlags[i], (s32)0, ownExtra, gained);
    }
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.ship.stats.entrancesDiscovered); i++) {
        MergeBits(gSaveContext.ship.stats.entrancesDiscovered[i], loadedData.ship.stats.entrancesDiscovered[i],
                  (u32)0, ownExtra, gained);
    }

    gSaveContext.ship.stats.firstInput = loadedData.ship.stats.firstInput;
    gSaveContext.ship.stats.fileCreatedAt = loadedData.ship.stats.fileCreatedAt;

    // Ensure ganon barrier state matches trials
    if (gSaveContext.eventChkInf[10] & 0x2000 && gSaveContext.eventChkInf[11] & 0xFC00) {
        gSaveContext.eventChkInf[12] |= 0x8;
    }

    // Hearts: whoever has more in total (containers and pieces), both values from the same side so a 4th piece
    // isn't counted twice
    Inventory& inv = gSaveContext.inventory;
    Inventory& their = loadedData.inventory;
    s32 ownHearts = gSaveContext.healthCapacity / 0x10 * 4 + ((inv.questItems & 0xF0000000) >> 28);
    s32 theirHearts = loadedData.healthCapacity / 0x10 * 4 + ((their.questItems & 0xF0000000) >> 28);
    if (theirHearts > ownHearts) {
        gained = true;
        if (loadedData.healthCapacity > gSaveContext.healthCapacity) {
            gSaveContext.health += loadedData.healthCapacity - gSaveContext.healthCapacity;
        }
        gSaveContext.healthCapacity = loadedData.healthCapacity;
        inv.questItems = (inv.questItems & ~0xF0000000) | (their.questItems & 0xF0000000);
    } else if (ownHearts > theirHearts) {
        ownExtra = true;
    }
    MergeBits(inv.questItems, their.questItems, (u32)0xF0000000, ownExtra, gained);

    s8 ownMagicLevel = gSaveContext.magicLevel;
    MergeMax(gSaveContext.magicLevel, loadedData.magicLevel, ownExtra, gained);
    MergeMax(gSaveContext.magicCapacity, loadedData.magicCapacity, ownExtra, gained);
    MergeMax(gSaveContext.isMagicAcquired, loadedData.isMagicAcquired, ownExtra, gained);
    MergeMax(gSaveContext.isDoubleMagicAcquired, loadedData.isDoubleMagicAcquired, ownExtra, gained);
    MergeMax(gSaveContext.isDoubleDefenseAcquired, loadedData.isDoubleDefenseAcquired, ownExtra, gained);
    MergeMax(inv.defenseHearts, their.defenseHearts, ownExtra, gained);
    MergeMax(gSaveContext.bgsFlag, loadedData.bgsFlag, ownExtra, gained);
    MergeMax(gSaveContext.swordHealth, loadedData.swordHealth, ownExtra, gained);
    if (gSaveContext.magicLevel > ownMagicLevel) {
        gSaveContext.magic = static_cast<s8>(gSaveContext.magicCapacity);
    }

    MergeBits(inv.equipment, their.equipment, (u16)0, ownExtra, gained);
    // Keep the bigger of each bag/quiver/wallet. A smaller one would cap our ammo
    for (int i = 0; i < ARRAY_COUNT(gUpgradeMasks); i++) {
        u32 own = inv.upgrades & gUpgradeMasks[i];
        u32 theirUpgrade = their.upgrades & gUpgradeMasks[i];
        ownExtra |= own > theirUpgrade;
        gained |= theirUpgrade > own;
        if (theirUpgrade > own) {
            inv.upgrades = (inv.upgrades & ~gUpgradeMasks[i]) | theirUpgrade;
        }
    }
    for (int i = 0; i < ARRAY_COUNT(inv.dungeonItems); i++) {
        MergeBits(inv.dungeonItems[i], their.dungeonItems[i], (u8)0, ownExtra, gained);
    }
    MergeMax(inv.gsTokens, their.gsTokens, ownExtra, gained);

    for (int i = 0; i < ARRAY_COUNT(inv.dungeonKeys); i++) {
        if (dungeonOwnExtra[i] && !dungeonGained[i]) {
            continue;
        } else if (dungeonGained[i] && !dungeonOwnExtra[i]) {
            inv.dungeonKeys[i] = their.dungeonKeys[i];
        } else {
            inv.dungeonKeys[i] = MAX(inv.dungeonKeys[i], their.dungeonKeys[i]);
        }
    }

    // Items: fill the slots we don't have. Hookshot, ocarina and the adult trade item only move forward, so the
    // further one wins. Bottle contents and the child trade slot (masks being sold and borrowed) are ours
    for (int i = 0; i < ARRAY_COUNT(inv.items); i++) {
        u8 theirItem = their.items[i];
        if (theirItem == ITEM_NONE) {
            ownExtra |= inv.items[i] != ITEM_NONE;
            continue;
        }
        bool isBottle = i >= SLOT_BOTTLE_1 && i <= SLOT_BOTTLE_4;
        if (inv.items[i] == ITEM_NONE) {
            gained = true;
            inv.items[i] = isBottle && IsBottleContent(theirItem) ? ITEM_BOTTLE : theirItem;
            if (i < ARRAY_COUNT(inv.ammo)) {
                inv.ammo[i] = their.ammo[i];
            }
        } else if (i == SLOT_HOOKSHOT || i == SLOT_OCARINA || i == SLOT_TRADE_ADULT) {
            MergeMax(inv.items[i], theirItem, ownExtra, gained);
        }
    }
    // Beans are one pool for the team (bought ones arrive through GIVE_ITEM, planted ones through
    // UPDATE_BEANS_COUNT), so take the team's count
    if (their.items[SLOT(ITEM_BEAN)] == ITEM_BEAN) {
        AMMO(ITEM_BEAN) = their.ammo[SLOT(ITEM_BEAN)];
        BEANS_BOUGHT = MAX(BEANS_BOUGHT, their.ammo[SLOT(ITEM_BEAN + 1)]);
    }

    if (IS_RANDO) {
        ShipRandomizerSaveContextData& mineRando = gSaveContext.ship.quest.data.randomizer;
        ShipRandomizerSaveContextData& theirRando = loadedData.ship.quest.data.randomizer;
        MergeMax(mineRando.triforcePiecesCollected, theirRando.triforcePiecesCollected, ownExtra, gained);
        MergeMax(mineRando.bombchuUpgradeLevel, theirRando.bombchuUpgradeLevel, ownExtra, gained);
#define SILVER_MERGE(field) MergeMax(mineRando.field, theirRando.field, ownExtra, gained);
        SILVER_RUPEE_FIELDS(SILVER_MERGE)
#undef SILVER_MERGE
    }

    if (IS_RANDO && state.contains("rando")) {
        for (int i = 0; i < RC_MAX; i++) {
            auto itemLocation = state["rando"].at("itemLocations").at(i);
            auto location = OTRGlobals::Instance->gRandoContext->GetItemLocation(i);
            RandomizerCheckStatus status = itemLocation.at(0).get<RandomizerCheckStatus>();
            ownExtra |= location->GetCheckStatus() > status;
            if (status > location->GetCheckStatus()) {
                location->SetCheckStatus(status);
            }
            location->SetIsSkipped(itemLocation.at(1).get<u8>());
        }
    }

    AnchorWorldSync::RefreshSnapshot();

    if (gained) {
        Notification::Emit({
            .message = "Save updated from team",
        });
    }
    return ownExtra;
}

void Anchor::HandlePacket_UpdateTeamState(nlohmann::json payload) {
    if (!roomState.syncItemsAndFlags) {
        return;
    }

    isHandlingUpdateTeamState = true;
    // This can happen in between file select and the game starting, so we can't use this check, but we need to ensure
    // we be careful to wrap PlayState usage in this check
    //
    // if (!IsSaveLoaded()) {
    //     return;
    // }

    if (payload.contains("state") && payload["state"].is_object() && !payload["state"].empty()) {
        if (MergeTeamState(payload["state"])) {
            // We have progress the team state lacks (played offline, or it's older than our save)
            pendingTeamStatePush = true;
        }
    }
    hasMergedTeamState = true;

    if (payload.contains("queue")) {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        for (auto& item : payload["queue"]) {
            nlohmann::json itemPayload = nlohmann::json::parse(item.get<std::string>());
            // Our own items are in our inventory since we got them (and the state above keeps them), so giving them
            // again would double them. Our own flags are replayed: the state above may have reset scene flags.
            if (itemPayload.value("clientId", 0u) == ownClientId && itemPayload.value("type", "") == GIVE_ITEM) {
                continue;
            }
            incomingPacketQueue.push(itemPayload);
        }
    }
    isHandlingUpdateTeamState = false;
}
