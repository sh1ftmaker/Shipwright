#include "WebJoin.h"

#include <string>
#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SaveManager.h"
#include "soh/Enhancements/boss-rush/BossRush.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
void Sram_InitNewSave(void);
void Sram_SetNewFileStart(void);
void Sram_InitBossRushSave(void);
void FileChoose_FinishLoadGame(GameState* thisx);
}

void OnZTitleUpdateSkipToFileSelect(void* gameState);
uint8_t encodePlayerNameChar(char c, uint8_t language);

static bool sPending = false;
static bool sBossRush = false;
static int sSlot = 0;
static std::string sName;
static HOOK_ID sTitleHook = 0;
static HOOK_ID sFileChooseHook = 0;

// Returns false if no character of `name` can be written in the file name font
static bool WebJoin_SetPlayerName(const std::string& name) {
    const uint8_t language = gSaveContext.ship.filenameLanguage;
    const uint8_t blank = encodePlayerNameChar(' ', language);
    bool visible = false;
    for (size_t i = 0; i < ARRAY_COUNT(gSaveContext.playerName); i++) {
        gSaveContext.playerName[i] = encodePlayerNameChar(i < name.size() ? name[i] : ' ', language);
        visible |= gSaveContext.playerName[i] != blank;
    }
    return visible;
}

static void WebJoin_CreateFile() {
    gSaveContext.fileNum = sSlot;
    Sram_InitNewSave();
    Sram_SetNewFileStart();

    if (!WebJoin_SetPlayerName(sName)) {
        WebJoin_SetPlayerName("Guest");
    }

    gSaveContext.n64ddFlag = 0;
    gSaveContext.ship.quest.id = ResourceMgr_GameHasOriginal() ? QUEST_NORMAL : QUEST_MASTER;

    SaveManager::Instance->SaveFile(sSlot);
    SaveManager::Instance->ThreadPoolWait();
}

static bool WebJoin_CanLoad(int slot) {
    const SaveFileMetaInfo* meta = &SaveManager::Instance->fileMetaInfo[slot];
    return meta->valid && (!meta->requiresMasterQuest || ResourceMgr_GameHasMasterQuest()) &&
           (!meta->requiresOriginal || ResourceMgr_GameHasOriginal());
}

static void WebJoin_Finish() {
    sPending = false;
    GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnZTitleUpdate>(sTitleHook);
    GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnFileChooseMain>(sFileChooseHook);
    sTitleHook = sFileChooseHook = 0;
}

// What starting Boss Rush from File Select does, with every option on its first choice
static void WebJoin_StartBossRush(GameState* state) {
    for (int i = 0; i < BR_OPTIONS_MAX; i++) {
        gSaveContext.ship.quest.data.bossRush.options[i] = 0;
    }
    gSaveContext.ship.quest.id = QUEST_BOSSRUSH;
    gSaveContext.fileNum = 0xFE;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    Sram_InitBossRushSave();
    SET_NEXT_GAMESTATE(state, Play_Init, PlayState);
    FileChoose_FinishLoadGame(state);
}

static void WebJoin_OnFileChooseMain(void* gameState) {
    GameState* state = (GameState*)gameState;

    if (sBossRush) {
        SPDLOG_INFO("[WebJoin] Starting Boss Rush");
        WebJoin_StartBossRush(state);
        WebJoin_Finish();
        return;
    }

    if (!SaveManager::Instance->SaveFile_Exist(sSlot)) {
        WebJoin_CreateFile();
    }

    if (!WebJoin_CanLoad(sSlot)) {
        SPDLOG_WARN("[WebJoin] Save slot {} can't be loaded, staying on File Select", sSlot + 1);
        WebJoin_Finish();
        return;
    }

    gSaveContext.fileNum = sSlot;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    Sram_OpenSave();
    SET_NEXT_GAMESTATE(state, Play_Init, PlayState);
    FileChoose_FinishLoadGame(state);

    WebJoin_Finish();
}

static void WebJoin_Request() {
    if (!sPending) {
        sPending = true;
        sTitleHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleUpdate>(
            OnZTitleUpdateSkipToFileSelect);
        sFileChooseHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFileChooseMain>(WebJoin_OnFileChooseMain);
    }
}

void WebJoin_RequestBootToSlot(int slot, const char* name) {
    if (slot < 0 || slot > 2) {
        SPDLOG_ERROR("[WebJoin] Invalid save slot {}", slot);
        return;
    }

    sSlot = slot;
    sName = std::string(name != nullptr ? name : "").substr(0, 8);
    sBossRush = false;
    WebJoin_Request();
}

void WebJoin_RequestBootToBossRush() {
    sBossRush = true;
    WebJoin_Request();
}

bool WebJoin_IsPending() {
    return sPending;
}
