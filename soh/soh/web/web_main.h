#ifndef WEB_MAIN_H
#define WEB_MAIN_H

#ifdef __EMSCRIPTEN__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Called from JavaScript when OTR files have been written to the Emscripten FS.
 */
void web_otr_loaded(void);

/**
 * Returns 1 if OTR files are loaded and available, 0 otherwise.
 */
int web_get_otr_status(void);

/**
 * Triggers FS.syncfs() to persist IDBFS data (saves, config) to IndexedDB.
 */
void web_save_to_idb(void);

/**
 * Initializes IDBFS mounts at /soh/save and /soh/config and loads persisted data.
 */
void web_fs_init(void);

/**
 * Extracts a .z64 ROM file into a .o2r archive using ZAPD.
 * @param romPath   Path to the ROM in MEMFS (e.g., "/rom.z64")
 * @param outputPath Path for the output .o2r file (e.g., "/oot.o2r")
 * @return 0 on success, negative error code on failure
 */
int web_extract_rom(const char* romPath, const char* outputPath);

/**
 * Loads a ROM directly into memory for DMA-based asset loading (no ZAPD extraction).
 * @param romPath Path to the ROM in MEMFS (e.g., "/rom.z64")
 * @return 0 = vanilla, 1 = MQ, negative = error
 */
int web_load_rom_direct(const char* romPath);

/**
 * Returns 1 if ROM direct loading is active, 0 otherwise.
 */
int web_get_rom_direct_status(void);

/**
 * Returns the ROM version string for a given ROM file.
 * @param romPath Path to the ROM in MEMFS
 * @return Static string with version info (e.g., "Vanilla", "MQ Master Quest")
 */
const char* web_get_rom_version(const char* romPath);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class RomArchive;
#include <memory>
/**
 * Returns the shared RomArchive instance if ROM direct loading is active.
 */
std::shared_ptr<RomArchive> web_get_rom_archive(void);
#endif

#endif /* __EMSCRIPTEN__ */

#endif /* WEB_MAIN_H */
