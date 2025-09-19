# Emscripten toolchain file for Ship of Harkinian

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_VERSION 1)

# Find Emscripten
if(NOT EMSCRIPTEN_ROOT_PATH)
    if(DEFINED ENV{EMSDK})
        set(EMSCRIPTEN_ROOT_PATH "$ENV{EMSDK}/upstream/emscripten")
    else()
        message(FATAL_ERROR "EMSDK environment variable not set. Please source emsdk_env.sh")
    endif()
endif()

set(CMAKE_C_COMPILER "${EMSCRIPTEN_ROOT_PATH}/emcc")
set(CMAKE_CXX_COMPILER "${EMSCRIPTEN_ROOT_PATH}/em++")
set(CMAKE_AR "${EMSCRIPTEN_ROOT_PATH}/emar")
set(CMAKE_RANLIB "${EMSCRIPTEN_ROOT_PATH}/emranlib")

# Set OpenGL ES for Emscripten
set(USE_OPENGLES ON CACHE BOOL "Use OpenGL ES" FORCE)

# Platform-specific settings
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Emscripten flags
set(EMSCRIPTEN_FLAGS "")
list(APPEND EMSCRIPTEN_FLAGS
    "-s USE_SDL=2"
    "-s USE_ZLIB=1"
    "-s USE_LIBPNG=1"
    "-s USE_WEBGL2=1"
    "-s FULL_ES3=1"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s MAXIMUM_MEMORY=4GB"
    "-s INITIAL_MEMORY=512MB"
    "-s ASSERTIONS=1"
    "-s WASM=1"
    "-s ASYNCIFY=1"
    "-s FETCH=1"
    "-s FORCE_FILESYSTEM=1"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','IDBFS']"
    "-s EXPORTED_FUNCTIONS=['_main','_malloc','_free']"
    "-s ENVIRONMENT=web"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME='createModule'"
    "-s SINGLE_FILE=0"
    "--preload-file assets@/"
    "--preload-file oot.o2r@/"
    "--preload-file oot-mq.o2r@/"
    "--preload-file soh.o2r@/"
    "-lidbfs.js"
)

string(REPLACE ";" " " EMSCRIPTEN_FLAGS_STRING "${EMSCRIPTEN_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EMSCRIPTEN_FLAGS_STRING}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EMSCRIPTEN_FLAGS_STRING}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EMSCRIPTEN_FLAGS_STRING}")

# Disable some features for web build
set(BUILD_CROWD_CONTROL OFF)
set(BUILD_REMOTE_CONTROL OFF)