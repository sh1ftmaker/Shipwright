#pragma once

#if defined(__EMSCRIPTEN__) && defined(ROM_DIRECT_LOADING)

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes audio data from ROM DMA entries.
 * Must be called after RomArchive is initialized, before AudioLoad_Init.
 *
 * Loads the Audiobank, Audioseq, and Audiotable DMA entries from ROM
 * into persistent memory and parses their AudioTable headers.
 *
 * Returns 0 on success, -1 on failure.
 */
int32_t RomDirectAudio_Init(void);

/**
 * Returns a pointer to raw sequence data for the given sequence ID.
 * @param seqId Sequence table index
 * @param outData Pointer to receive the data pointer
 * @param outSize Pointer to receive the data size
 * @return 0 on success, -1 if not available
 */
int32_t RomDirectAudio_GetSequenceData(uint32_t seqId, const uint8_t** outData, uint32_t* outSize);

/**
 * Returns the number of sequences in the ROM's Audioseq table.
 */
uint32_t RomDirectAudio_GetSequenceCount(void);

/**
 * Returns the number of sound fonts in the ROM's Audiobank table.
 */
uint32_t RomDirectAudio_GetFontCount(void);

/**
 * Returns the number of sample banks in the ROM's Audiotable table.
 */
uint32_t RomDirectAudio_GetSampleBankCount(void);

/**
 * Returns whether ROM direct audio has been initialized.
 */
int32_t RomDirectAudio_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif // __EMSCRIPTEN__ && ROM_DIRECT_LOADING
