#if defined(__EMSCRIPTEN__) && defined(ROM_DIRECT_LOADING)

#include "RomDirect.h"
#include "RomArchive.h"
#include "web_main.h"

#include <cstdio>
#include <cstring>

// Game headers
extern "C" {
#include "z64scene.h"
#include "z64.h"
}

// External game tables
extern "C" {
    extern RomFile gObjectTable[];
    extern u32 gObjectTableSize;
    extern SceneTableEntry gSceneTable[];
}

extern "C" int32_t RomDirect_DmaLoad(void* dest, uintptr_t vrom, size_t size) {
    auto romArchive = web_get_rom_archive();
    if (!romArchive || !romArchive->GetDma()) {
        return -1;
    }

    auto data = romArchive->GetDma()->LoadVrom((uint32_t)vrom, (uint32_t)(vrom + size));
    if (data.empty()) {
        fprintf(stderr, "[RomDirect] DMA load failed for VROM 0x%lX size 0x%zX\n",
                (unsigned long)vrom, size);
        return -1;
    }

    // Copy to destination, clamping to requested size
    size_t copySize = data.size() < size ? data.size() : size;
    memcpy(dest, data.data(), copySize);
    return 0;
}

extern "C" void RomDirect_PopulateObjectTable(void) {
    auto romArchive = web_get_rom_archive();
    if (!romArchive || !romArchive->GetDma()) {
        fprintf(stderr, "[RomDirect] Cannot populate object table — no ROM archive\n");
        return;
    }

    RomDma* dma = romArchive->GetDma();
    size_t populated = 0;

    for (uint32_t i = 0; i < gObjectTableSize; i++) {
        if (gObjectTable[i].fileName == NULL || gObjectTable[i].fileName[0] == '\0') {
            continue;
        }

        // Already has VROM addresses
        if (gObjectTable[i].vromStart != 0) {
            populated++;
            continue;
        }

        // Look up the DMA index by filename (matches file list entries)
        const char* name = gObjectTable[i].fileName;
        size_t dmaIndex = romArchive->FindDmaIndexByName(std::string(name));
        if (dmaIndex != SIZE_MAX) {
            const auto& entry = dma->GetEntry(dmaIndex);
            gObjectTable[i].vromStart = entry.virtStart;
            gObjectTable[i].vromEnd = entry.virtEnd;
            populated++;
        }
    }

    printf("[RomDirect] Populated %zu/%u object table entries with VROM addresses\n",
           populated, gObjectTableSize);
}

extern "C" void RomDirect_PopulateSceneTable(void) {
    auto romArchive = web_get_rom_archive();
    if (!romArchive || !romArchive->GetDma()) {
        fprintf(stderr, "[RomDirect] Cannot populate scene table — no ROM archive\n");
        return;
    }

    RomDma* dma = romArchive->GetDma();

    // Scene table size is not exposed as a variable, but we know
    // the number of scenes from SCENE_ID_MAX (or iterate until empty)
    // We'll iterate a safe max and stop at empty entries.
    size_t populated = 0;
    for (int i = 0; i < 110; i++) { // OoT has ~101 scenes
        SceneTableEntry* scene = &gSceneTable[i];

        // Populate sceneFile VROM addresses
        if (scene->sceneFile.fileName != NULL && scene->sceneFile.fileName[0] != '\0'
            && scene->sceneFile.vromStart == 0) {
            size_t dmaIndex = romArchive->FindDmaIndexByName(
                std::string(scene->sceneFile.fileName));
            if (dmaIndex != SIZE_MAX) {
                const auto& entry = dma->GetEntry(dmaIndex);
                scene->sceneFile.vromStart = entry.virtStart;
                scene->sceneFile.vromEnd = entry.virtEnd;
                populated++;
            }
        } else if (scene->sceneFile.vromStart != 0) {
            populated++;
        }

        // Populate titleFile VROM addresses
        if (scene->titleFile.fileName != NULL && scene->titleFile.fileName[0] != '\0'
            && scene->titleFile.vromStart == 0) {
            size_t dmaIndex = romArchive->FindDmaIndexByName(
                std::string(scene->titleFile.fileName));
            if (dmaIndex != SIZE_MAX) {
                const auto& entry = dma->GetEntry(dmaIndex);
                scene->titleFile.vromStart = entry.virtStart;
                scene->titleFile.vromEnd = entry.virtEnd;
            }
        }
    }

    printf("[RomDirect] Populated %zu scene table entries with VROM addresses\n", populated);
}

#endif // __EMSCRIPTEN__ && ROM_DIRECT_LOADING
