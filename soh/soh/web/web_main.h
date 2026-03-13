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
 * Returns the ROM version string for a given ROM file.
 * @param romPath Path to the ROM in MEMFS
 * @return Static string with version info (e.g., "Vanilla", "MQ Master Quest")
 */
const char* web_get_rom_version(const char* romPath);

/**
 * Configures Anchor/PartyKit networking from URL hash parameters.
 * Sets CVars for room, name, color, team and enables Anchor.
 * Also sets boot sequence to file select for quick game start.
 */
void web_configure_anchor(const char* room, const char* name, const char* color, const char* team);

#ifdef __cplusplus
}
#endif

#endif /* __EMSCRIPTEN__ */

#endif /* WEB_MAIN_H */
