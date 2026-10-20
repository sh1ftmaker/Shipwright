#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/game-interactor/GameInteractionEffect.h"

extern "C" {
#include "functions.h"
#include "soh/Enhancements/randomizer/ShuffleTradeItems.h"
extern PlayState* gPlayState;
}

/**
 * UNSET_FLAG
 *
 * Fired when a flag is unset in the save context
 */

void Anchor::SendPacket_UnsetFlag(s16 sceneNum, s16 flagType, s16 flag) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags ||
        AnchorWorldSync::IsPerPlayerFlag(sceneNum, flagType, flag)) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = UNSET_FLAG;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    // Temp flags are gone by the time anyone replays the queue
    payload["addToQueue"] = !AnchorWorldSync::IsTempFlag(sceneNum, flagType, flag);
    payload["sceneNum"] = sceneNum;
    payload["flagType"] = flagType;
    payload["flag"] = flag;
    payload["roomNum"] = gPlayState->roomCtx.curRoom.num;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UnsetFlag(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    s16 sceneNum = payload.at("sceneNum").get<s16>();
    s16 flagType = payload.at("flagType").get<s16>();
    s16 flag = payload.at("flag").get<s16>();

    // sceneNum == SCENE_ID_MAX is a sentinel meaning "global flag" (handled below); only larger
    // values would index gSaveContext.sceneFlags out of bounds.
    if (sceneNum < 0 || sceneNum > SCENE_ID_MAX) {
        SPDLOG_ERROR("[Anchor] UNSET_FLAG: sceneNum {} out of range", sceneNum);
        return;
    }

    if (AnchorWorldSync::IsPerPlayerFlag(sceneNum, flagType, flag)) {
        return;
    }

    // Room-local temp flags only mean something in the room that set them
    if (AnchorWorldSync::IsRoomLocalFlag(flagType, flag) &&
        (sceneNum != gPlayState->sceneNum ||
         payload.value("roomNum", (s16)gPlayState->roomCtx.curRoom.num) != gPlayState->roomCtx.curRoom.num)) {
        return;
    }

    if (sceneNum == SCENE_ID_MAX) {
        auto effect = new GameInteractionEffect::UnsetFlag();
        effect->parameters[0] = flagType;
        effect->parameters[1] = flag;
        effect->Apply();

        // Special case: If an adult trade item flag is unset, replace the item if the player has it equipped
        if (flagType == FLAG_RANDOMIZER_INF &&
            (flag >= RAND_INF_ADULT_TRADES_HAS_POCKET_EGG && flag <= RAND_INF_ADULT_TRADES_HAS_CLAIM_CHECK)) {
            u16 itemToReplace = ITEM_POCKET_EGG;
            switch (flag) {
                case RAND_INF_ADULT_TRADES_HAS_POCKET_EGG:
                    itemToReplace = ITEM_POCKET_EGG;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_POCKET_CUCCO:
                    itemToReplace = ITEM_POCKET_CUCCO;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_COJIRO:
                    itemToReplace = ITEM_COJIRO;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_ODD_MUSHROOM:
                    itemToReplace = ITEM_ODD_MUSHROOM;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_ODD_POTION:
                    itemToReplace = ITEM_ODD_POTION;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_SAW:
                    itemToReplace = ITEM_SAW;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_SWORD_BROKEN:
                    itemToReplace = ITEM_SWORD_BROKEN;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_PRESCRIPTION:
                    itemToReplace = ITEM_PRESCRIPTION;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_FROG:
                    itemToReplace = ITEM_FROG;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_EYEDROPS:
                    itemToReplace = ITEM_EYEDROPS;
                    break;
                case RAND_INF_ADULT_TRADES_HAS_CLAIM_CHECK:
                    itemToReplace = ITEM_CLAIM_CHECK;
                    break;
            }
            Inventory_ReplaceItem(gPlayState, itemToReplace, Randomizer_GetNextAdultTradeItem());
        }
    } else {
        auto effect = new GameInteractionEffect::UnsetSceneFlag();
        effect->parameters[0] = sceneNum;
        effect->parameters[1] = flagType;
        effect->parameters[2] = flag;
        effect->Apply();
    }
}
