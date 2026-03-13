#ifdef __EMSCRIPTEN__

#include <stdio.h>
#include <string.h>
#include <fstream>
#include <atomic>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "web_main.h"
#include "soh/Extractor/Extract.h"
#include <ship/utils/binarytools/BitConverter.h>
#include <libultraship/libultraship.h>
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* gPlayState;
void Sram_InitNewSave(void);
void GameInteractor_ExecuteOnLoadGame(int32_t fileNum);
}

// ---- Touch Gamepad Bridge ----
// Read touch gamepad state from JavaScript and merge into OSContPad

// N64 button masks (from libultra/os.h)
#define N64_A      0x8000
#define N64_B      0x4000
#define N64_Z      0x2000
#define N64_START  0x1000
#define N64_L      0x0020
#define N64_R      0x0010
#define N64_CU     0x0008
#define N64_CD     0x0004
#define N64_CL     0x0002
#define N64_CR     0x0001

EM_JS(int, web_touch_active, (), {
    return (typeof TouchGamepad !== 'undefined' && TouchGamepad.isActive()) ? 1 : 0;
});

EM_JS(int, web_touch_stick_x, (), {
    return (typeof TouchGamepad !== 'undefined') ? TouchGamepad.getStickX() : 0;
});

EM_JS(int, web_touch_stick_y, (), {
    return (typeof TouchGamepad !== 'undefined') ? TouchGamepad.getStickY() : 0;
});

EM_JS(int, web_touch_buttons, (), {
    if (typeof TouchGamepad === 'undefined' || !TouchGamepad.isActive()) return 0;
    var b = 0;
    if (TouchGamepad.isButtonPressed('tb-a'))     b |= 0x8000;
    if (TouchGamepad.isButtonPressed('tb-b'))     b |= 0x4000;
    if (TouchGamepad.isButtonPressed('tb-z'))     b |= 0x2000;
    if (TouchGamepad.isButtonPressed('tb-start')) b |= 0x1000;
    if (TouchGamepad.isButtonPressed('tb-l'))     b |= 0x0020;
    if (TouchGamepad.isButtonPressed('tb-r'))     b |= 0x0010;
    if (TouchGamepad.isButtonPressed('tb-cu'))    b |= 0x0008;
    if (TouchGamepad.isButtonPressed('tb-cd'))    b |= 0x0004;
    if (TouchGamepad.isButtonPressed('tb-cl'))    b |= 0x0002;
    if (TouchGamepad.isButtonPressed('tb-cr'))    b |= 0x0001;
    return b;
});

#include <libultraship/libultra/controller.h>

extern "C" void WebTouchGamepad_MergeInput(OSContPad* pad) {
    if (!web_touch_active()) return;

    // Merge buttons (OR with existing)
    pad->button |= (uint16_t)web_touch_buttons();

    // Merge stick (touch overrides if non-zero)
    int sx = web_touch_stick_x();
    int sy = web_touch_stick_y();
    if (sx != 0 || sy != 0) {
        pad->stick_x = (int8_t)sx;
        pad->stick_y = (int8_t)sy;
    }
}

// Convert ASCII character to OoT NES font encoding
static uint8_t AsciiToOot(char c) {
    if (c >= 'A' && c <= 'Z') return 0xAB + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0xAB + (c - 'a');
    if (c >= '0' && c <= '9') return 0xA1 + (c - '0');
    if (c == ' ') return 0xDF;
    if (c == '-') return 0xE4;
    if (c == '.') return 0xE5;
    return 0xDF; // space for unknown chars
}

#define CVAR_WEB_AUTOSTART CVAR_SETTING("WebAutoStart")
#define CVAR_WEB_PLAYERNAME CVAR_SETTING("WebPlayerName")

static void RegisterWebAutoStart() {
    COND_HOOK(OnZTitleUpdate, CVarGetInteger(CVAR_WEB_AUTOSTART, 0) == 1, [](void* gameState) {
        TitleContext* titleContext = (TitleContext*)gameState;

        // Create a fresh new save on file slot 0
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gSaveContext.fileNum = 0;
        Sram_InitNewSave();

        // Set player name from CVar
        const char* playerName = CVarGetString(CVAR_WEB_PLAYERNAME, "LINK");
        for (int i = 0; i < 8; i++) {
            if (playerName[i] == '\0') {
                // Pad rest with spaces
                for (int j = i; j < 8; j++) {
                    gSaveContext.playerName[j] = AsciiToOot(' ');
                }
                break;
            }
            gSaveContext.playerName[i] = AsciiToOot(playerName[i]);
        }

        // Set up save state for starting a new game
        gSaveContext.magicFillTarget = gSaveContext.magic;
        gSaveContext.magic = 0;
        gSaveContext.magicCapacity = 0;
        gSaveContext.magicLevel = gSaveContext.magic;
        gSaveContext.sceneSetupIndex = 0;
        gSaveContext.cutsceneIndex = 0xFFF3; // Skip intro cutscene
        gSaveContext.linkAge = 0; // Child Link
        gSaveContext.nightFlag = 0;
        gSaveContext.skyboxTime = gSaveContext.dayTime = 0x8000;
        gSaveContext.entranceIndex = ENTR_LINKS_HOUSE_CHILD_SPAWN;

        for (int i = 0; i < ARRAY_COUNT(gSaveContext.buttonStatus); i++) {
            gSaveContext.buttonStatus[i] = BTN_ENABLED;
        }
        gSaveContext.forceRisingButtonAlphas = gSaveContext.unk_13E8 =
            gSaveContext.unk_13EA = gSaveContext.unk_13EC = 0;
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_STOP);

        gSaveContext.seqId = (u8)NA_BGM_DISABLED;
        gSaveContext.natureAmbienceId = 0xFF;
        gSaveContext.showTitleCard = true;
        gWeatherMode = 0;
        titleContext->state.running = false;
        SET_NEXT_GAMESTATE(&titleContext->state, Play_Init, PlayState);
        GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);

        printf("[Web] Auto-started new game as '%s'\n", playerName);
    });
}

static RegisterShipInitFunc webAutoStartInit(RegisterWebAutoStart, { CVAR_WEB_AUTOSTART });

static int s_otr_loaded = 0;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void web_otr_loaded(void) {
    s_otr_loaded = 1;
    printf("[Web] OTR files loaded into virtual filesystem.\n");
}

EMSCRIPTEN_KEEPALIVE
int web_get_otr_status(void) {
    return s_otr_loaded;
}

EMSCRIPTEN_KEEPALIVE
void web_save_to_idb(void) {
    emscripten_run_script(
        "if (typeof FS !== 'undefined' && FS.syncfs) {"
        "  FS.syncfs(false, function(err) {"
        "    if (err) console.error('[Web] FS.syncfs save failed:', err);"
        "    else console.log('[Web] FS.syncfs save complete.');"
        "  });"
        "}"
    );
}

EM_JS(void, web_mount_idbfs, (), {
    var dirs = ["/soh/save", "/soh/config"];
    for (var i = 0; i < dirs.length; i++) {
        try { FS.mkdir(dirs[i]); } catch (e) { /* may exist */ }
        FS.mount(IDBFS, {}, dirs[i]);
    }
    FS.syncfs(true, function(err) {
        if (err) console.error("[Web] IDBFS load failed:", err);
        else console.log("[Web] IDBFS loaded.");
    });
});

void web_fs_init(void) {
    // Ensure the /soh directory exists
    EM_ASM(
        try { FS.mkdir('/soh'); } catch(e) { /* exists */ }
    );
    web_mount_idbfs();
    printf("[Web] IDBFS mounts initialized for /soh/save, /soh/config.\n");
}

EMSCRIPTEN_KEEPALIVE
int web_extract_rom(const char* romPath, const char* outputPath) {
    printf("[Web] Starting ROM extraction: %s -> %s\n", romPath, outputPath);

    Extractor extractor;

    // Use RunFileStandalone to validate and set up the extractor
    if (!extractor.RunFileStandalone(std::string(romPath))) {
        fprintf(stderr, "[Web] ROM validation failed\n");
        return -3;
    }

    bool isMQ = extractor.IsMasterQuest();
    const char* expectedFile = isMQ ? "oot-mq.o2r" : "oot.o2r";
    printf("[Web] ROM validated. Version: %s\n", isMQ ? "Master Quest" : "Vanilla");

    // Run ZAPD extraction — output goes to "/" + expectedFile
    std::atomic<size_t> extractCount(0);
    std::atomic<size_t> totalExtract(0);
    extractor.CallZapd("/", "/", &extractCount, &totalExtract);

    // Verify output was created
    std::string actualOutput = std::string("/") + expectedFile;
    std::ifstream checkFile(actualOutput, std::ios::binary | std::ios::ate);
    if (!checkFile.is_open() || checkFile.tellg() == 0) {
        fprintf(stderr, "[Web] Extraction failed - output file not created: %s\n", actualOutput.c_str());
        return -4;
    }

    printf("[Web] Extraction complete. Output: %s (%lld bytes)\n", actualOutput.c_str(), (long long)checkFile.tellg());
    return isMQ ? 1 : 0; // 0 = vanilla success, 1 = MQ success, negative = error
}

EMSCRIPTEN_KEEPALIVE
const char* web_get_rom_version(const char* romPath) {
    static char versionBuf[64];
    versionBuf[0] = '\0';

    Extractor extractor;
    if (!extractor.RunFileStandalone(std::string(romPath))) {
        snprintf(versionBuf, sizeof(versionBuf), "unknown");
        return versionBuf;
    }

    snprintf(versionBuf, sizeof(versionBuf), "%s%s",
             extractor.IsMasterQuest() ? "MQ " : "",
             extractor.IsMasterQuest() ? "Master Quest" : "Vanilla");
    return versionBuf;
}

EMSCRIPTEN_KEEPALIVE
void web_configure_anchor(const char* room, const char* name, const char* color, const char* team) {
    printf("[Web] Configuring Anchor: room=%s name=%s color=%s team=%s\n",
           room ? room : "(null)", name ? name : "(null)",
           color ? color : "(null)", team ? team : "(null)");

    // Build WebSocket URL with room name
    std::string roomId = (room && room[0]) ? room : "default";
    std::string wsUrl = "wss://soh-anchor.zalo.partykit.dev/party/" + roomId;
    CVarSetString(CVAR_REMOTE_ANCHOR("WebSocketURL"), wsUrl.c_str());
    CVarSetString(CVAR_REMOTE_ANCHOR("RoomId"), roomId.c_str());

    // Player name
    if (name && name[0]) {
        CVarSetString(CVAR_REMOTE_ANCHOR("Name"), name);
    }

    // Player color (hex RGB like "FF0000")
    if (color && color[0]) {
        unsigned int r = 100, g = 255, b = 100;
        if (strlen(color) == 6) {
            sscanf(color, "%02x%02x%02x", &r, &g, &b);
        }
        // Color CVar is stored as a packed 24-bit RGB value
        uint32_t colorVal = (r << 16) | (g << 8) | b;
        CVarSetColor24(CVAR_REMOTE_ANCHOR("Color.Value"), { (uint8_t)r, (uint8_t)g, (uint8_t)b });
    }

    // Team ID
    if (team && team[0]) {
        CVarSetString(CVAR_REMOTE_ANCHOR("TeamId"), team);
    }

    // Enable Anchor networking
    CVarSetInteger(CVAR_REMOTE_ANCHOR("Enabled"), 1);

    // Set player name for auto-start save file
    std::string pName = (name && name[0]) ? name : "LINK";
    CVarSetString(CVAR_WEB_PLAYERNAME, pName.c_str());

    // Enable auto-start: skip title and file select, boot directly into the game
    CVarSetInteger(CVAR_WEB_AUTOSTART, 1);

    printf("[Web] Anchor configured. WebSocket URL: %s, auto-start as '%s'\n",
           wsUrl.c_str(), pName.c_str());
}

} // extern "C"

#endif /* __EMSCRIPTEN__ */
