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

    // Skip to file select on boot so players get into the game quickly
    CVarSetInteger(CVAR_SETTING("BootSequence"), BOOTSEQUENCE_FILESELECT);

    printf("[Web] Anchor configured. WebSocket URL: %s\n", wsUrl.c_str());
}

} // extern "C"

#endif /* __EMSCRIPTEN__ */
