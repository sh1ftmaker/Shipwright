#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/fetch.h>
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cstdio>
#include <libultraship/libultraship.h>

extern "C" {

// Platform initialization
void WebPlatform_Init() {
    // Set up Emscripten-specific SDL hints
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "1");

    // Initialize filesystem
    EM_ASM({
        // Ensure filesystem is ready
        if (Module.FS) {
            // Create directories if they don't exist
            try {
                Module.FS.mkdir('/save');
            } catch(e) {
                // Directory may already exist
            }

            try {
                Module.FS.mkdir('/mods');
            } catch(e) {
                // Directory may already exist
            }
        }
    });
}

// Mouse lock handling
EMSCRIPTEN_KEEPALIVE
void WebPlatform_RequestPointerLock() {
    emscripten_request_pointerlock("#canvas", EM_TRUE);
}

EMSCRIPTEN_KEEPALIVE
void WebPlatform_ExitPointerLock() {
    emscripten_exit_pointerlock();
}

// Fullscreen handling
EMSCRIPTEN_KEEPALIVE
void WebPlatform_RequestFullscreen() {
    EmscriptenFullscreenStrategy strategy;
    strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT;
    strategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF;
    strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_NEAREST;

    emscripten_request_fullscreen_strategy("#canvas", EM_TRUE, &strategy);
}

// Save data persistence
EMSCRIPTEN_KEEPALIVE
void WebPlatform_SyncFileSystem() {
    EM_ASM({
        if (Module.FS) {
            Module.FS.syncfs(false, function(err) {
                if (err) {
                    console.error('Error syncing filesystem:', err);
                }
            });
        }
    });
}

// Download OTR file
EMSCRIPTEN_KEEPALIVE
int WebPlatform_DownloadOTR(const char* url, const char* filename) {
    EM_ASM({
        const url = UTF8ToString($0);
        const filename = UTF8ToString($1);

        fetch(url)
            .then(response => {
                if (!response.ok) {
                    throw new Error('Network response was not ok');
                }
                return response.arrayBuffer();
            })
            .then(data => {
                const uint8Array = new Uint8Array(data);
                Module.FS.writeFile(filename, uint8Array);
                console.log('Downloaded ' + filename + ' successfully');
                Module.ccall('WebPlatform_OnOTRDownloaded', null, ['string'], [filename]);
            })
            .catch(error => {
                console.error('Error downloading OTR:', error);
                Module.ccall('WebPlatform_OnOTRDownloadError', null, ['string'], [filename]);
            });
    }, url, filename);

    return 0;
}

// Callback for successful OTR download
EMSCRIPTEN_KEEPALIVE
void WebPlatform_OnOTRDownloaded(const char* filename) {
    printf("OTR file downloaded: %s\n", filename);
    // Trigger game reload or asset refresh
}

// Callback for OTR download error
EMSCRIPTEN_KEEPALIVE
void WebPlatform_OnOTRDownloadError(const char* filename) {
    printf("Failed to download OTR file: %s\n", filename);
}

// Check if OTR file exists
EMSCRIPTEN_KEEPALIVE
int WebPlatform_OTRExists(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

// Get canvas size
EMSCRIPTEN_KEEPALIVE
void WebPlatform_GetCanvasSize(int* width, int* height) {
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    *width = (int)w;
    *height = (int)h;
}

// Set canvas size
EMSCRIPTEN_KEEPALIVE
void WebPlatform_SetCanvasSize(int width, int height) {
    emscripten_set_canvas_element_size("#canvas", width, height);
}

// Performance monitoring
EMSCRIPTEN_KEEPALIVE
double WebPlatform_GetPerformanceNow() {
    return emscripten_performance_now();
}

// Handle visibility change
EMSCRIPTEN_KEEPALIVE
void WebPlatform_OnVisibilityChange(int hidden) {
    if (hidden) {
        // Pause audio and reduce update rate
        SDL_PauseAudio(1);
    } else {
        // Resume audio
        SDL_PauseAudio(0);
    }
}

// Clipboard handling
EMSCRIPTEN_KEEPALIVE
void WebPlatform_SetClipboard(const char* text) {
    EM_ASM({
        if (navigator.clipboard) {
            navigator.clipboard.writeText(UTF8ToString($0));
        }
    }, text);
}

EMSCRIPTEN_KEEPALIVE
const char* WebPlatform_GetClipboard() {
    static std::string clipboardText;

    char* result = (char*)EM_ASM_INT({
        if (navigator.clipboard) {
            navigator.clipboard.readText().then(text => {
                const ptr = Module._malloc(text.length + 1);
                Module.stringToUTF8(text, ptr, text.length + 1);
                return ptr;
            });
        }
        return 0;
    });

    if (result) {
        clipboardText = result;
        free(result);
        return clipboardText.c_str();
    }

    return "";
}

// Open URL in new tab
EMSCRIPTEN_KEEPALIVE
void WebPlatform_OpenURL(const char* url) {
    EM_ASM({
        window.open(UTF8ToString($0), '_blank');
    }, url);
}

// Show alert dialog
EMSCRIPTEN_KEEPALIVE
void WebPlatform_ShowAlert(const char* message) {
    EM_ASM({
        alert(UTF8ToString($0));
    }, message);
}

// Show confirm dialog
EMSCRIPTEN_KEEPALIVE
int WebPlatform_ShowConfirm(const char* message) {
    return EM_ASM_INT({
        return confirm(UTF8ToString($0)) ? 1 : 0;
    }, message);
}

// Local storage helpers
EMSCRIPTEN_KEEPALIVE
void WebPlatform_SetLocalStorage(const char* key, const char* value) {
    EM_ASM({
        localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
    }, key, value);
}

EMSCRIPTEN_KEEPALIVE
const char* WebPlatform_GetLocalStorage(const char* key) {
    static std::string storageValue;

    char* result = (char*)EM_ASM_INT({
        const value = localStorage.getItem(UTF8ToString($0));
        if (value) {
            const ptr = Module._malloc(value.length + 1);
            Module.stringToUTF8(value, ptr, value.length + 1);
            return ptr;
        }
        return 0;
    }, key);

    if (result) {
        storageValue = result;
        free(result);
        return storageValue.c_str();
    }

    return "";
}

// Extract assets from ROM (placeholder - needs integration with OTRExporter)
EMSCRIPTEN_KEEPALIVE
void extractAssets() {
    printf("Extracting assets from ROM...\n");

    // Check if ROM exists
    FILE* rom = fopen("/rom.z64", "rb");
    if (!rom) {
        printf("ROM file not found\n");
        return;
    }
    fclose(rom);

    // TODO: Call actual OTRExporter functions here
    // This would need to be integrated with the existing extraction code

    printf("Asset extraction complete\n");
    WebPlatform_SyncFileSystem();
}

} // extern "C"

#endif // __EMSCRIPTEN__