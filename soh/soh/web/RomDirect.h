#pragma once

#if defined(__EMSCRIPTEN__) && defined(ROM_DIRECT_LOADING)

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Performs a DMA load from ROM via the DMA table.
 * Used by DmaMgr_SendRequest1 for ROM direct loading.
 * @param dest Destination buffer
 * @param vrom VROM address to load from
 * @param size Size to load
 * @return 0 on success, -1 on failure
 */
int32_t RomDirect_DmaLoad(void* dest, uintptr_t vrom, size_t size);

/**
 * Populates gObjectTable entries with VROM addresses from the ROM DMA table.
 * Must be called after RomArchive is initialized and before Object_Spawn.
 */
void RomDirect_PopulateObjectTable(void);

/**
 * Populates scene table entries with VROM addresses from the ROM DMA table.
 */
void RomDirect_PopulateSceneTable(void);

#ifdef __cplusplus
}
#endif

#endif // __EMSCRIPTEN__ && ROM_DIRECT_LOADING
