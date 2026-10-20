#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Enhancements/game-interactor/GameInteractionEffect.h"

extern "C" {
#include "functions.h"
extern PlayState* gPlayState;
}

/**
 * SET_FLAG
 *
 * Fired when a flag is set in the save context
 */

void Anchor::SendPacket_SetFlag(s16 sceneNum, s16 flagType, s16 flag) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags ||
        AnchorWorldSync::IsPerPlayerFlag(sceneNum, flagType, flag)) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = SET_FLAG;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    // Temp flags are gone by the time anyone replays the queue
    payload["addToQueue"] = !AnchorWorldSync::IsTempFlag(sceneNum, flagType, flag);
    payload["sceneNum"] = sceneNum;
    payload["flagType"] = flagType;
    payload["flag"] = flag;
    payload["roomNum"] = gPlayState->roomCtx.curRoom.num;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_SetFlag(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    s16 sceneNum = payload.at("sceneNum").get<s16>();
    s16 flagType = payload.at("flagType").get<s16>();
    s16 flag = payload.at("flag").get<s16>();

    // sceneNum == SCENE_ID_MAX is a sentinel meaning "global flag" (handled below); only larger
    // values would index gSaveContext.sceneFlags out of bounds.
    if (sceneNum < 0 || sceneNum > SCENE_ID_MAX) {
        SPDLOG_ERROR("[Anchor] SET_FLAG: sceneNum {} out of range", sceneNum);
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

    AnchorWorldSync::FlagRef ref = { sceneNum, flagType, flag };
    if (!AnchorWorldSync::IsFlagSet(ref)) {
        AnchorWorldSync::MarkSetByTeammate(ref);
    }

    if (sceneNum == SCENE_ID_MAX) {
        auto effect = new GameInteractionEffect::SetFlag();
        effect->parameters[0] = flagType;
        effect->parameters[1] = flag;
        effect->Apply();

        // Special case: If King Zora moved, and the player has Ruto's Letter, convert it to an empty bottle
        if (flagType == FLAG_EVENT_CHECK_INF && flag == EVENTCHKINF_KING_ZORA_MOVED &&
            Inventory_HasSpecificBottle(ITEM_LETTER_RUTO)) {
            Inventory_ReplaceItem(gPlayState, ITEM_LETTER_RUTO, ITEM_BOTTLE);
        }
    } else {
        auto effect = new GameInteractionEffect::SetSceneFlag();
        effect->parameters[0] = sceneNum;
        effect->parameters[1] = flagType;
        effect->parameters[2] = flag;
        effect->Apply();
    }
}
