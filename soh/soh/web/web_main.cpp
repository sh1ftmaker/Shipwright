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
#include "RomDma.h"
#include "RomArchive.h"

static int s_otr_loaded = 0;
static int s_rom_direct_loaded = 0;
static std::shared_ptr<RomArchive> s_romArchive = nullptr;

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
int web_load_rom_direct(const char* romPath) {
    printf("[Web] ROM direct loading: %s\n", romPath);

    // Read the ROM file from the Emscripten virtual filesystem
    std::ifstream romFile(romPath, std::ios::binary | std::ios::ate);
    if (!romFile.is_open()) {
        fprintf(stderr, "[Web] Failed to open ROM file: %s\n", romPath);
        return -1;
    }

    size_t romSize = romFile.tellg();
    romFile.seekg(0);

    std::vector<uint8_t> romData(romSize);
    romFile.read(reinterpret_cast<char*>(romData.data()), romSize);
    romFile.close();

    printf("[Web] ROM loaded: %zu bytes\n", romSize);

    // Quick CRC validation
    if (romSize < 0x40) {
        fprintf(stderr, "[Web] ROM too small\n");
        return -2;
    }

    // Create RomDma to validate the ROM can be parsed
    RomDma dmaCheck(romData.data(), romSize);
    uint32_t crc = dmaCheck.GetRomCrc();
    printf("[Web] ROM CRC: 0x%08X\n", crc);

    if (!dmaCheck.ParseDmaTable()) {
        fprintf(stderr, "[Web] Failed to parse DMA table — ROM may be invalid\n");
        return -3;
    }

    bool isMQ = dmaCheck.IsMasterQuest();
    printf("[Web] ROM version: %s (%zu DMA entries)\n",
           isMQ ? "Master Quest" : "Vanilla",
           dmaCheck.GetEntryCount());

    // Create the RomArchive (moves ROM data ownership)
    s_romArchive = std::make_shared<RomArchive>("rom://direct", std::move(romData));

    s_rom_direct_loaded = 1;
    s_otr_loaded = 1; // Signal that game files are ready
    printf("[Web] ROM direct loading complete. Game can start.\n");

    return isMQ ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int web_get_rom_direct_status(void) {
    return s_rom_direct_loaded;
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

} // extern "C"

// C++ function (not extern "C")
std::shared_ptr<RomArchive> web_get_rom_archive(void) {
    return s_romArchive;
}

#endif /* __EMSCRIPTEN__ */
