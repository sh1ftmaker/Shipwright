#ifdef __EMSCRIPTEN__

#include "RomArchive.h"
#include "ship/Context.h"
#include "ship/resource/File.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/ArchiveManager.h"
#include "ship/utils/StrHash64.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

// The file lists map DMA entry names by index (one per line).
// We embed the appropriate file list at build time via --preload-file,
// or load it from the Emscripten virtual filesystem.
static const char* sFileListPaths[] = {
    "/soh/assets/extractor/filelists/gamecube.txt",
    "/soh/assets/extractor/filelists/ntsc_oot.txt",
    "/soh/assets/extractor/filelists/pal_oot.txt",
    "/soh/assets/extractor/filelists/ntsc_12_oot.txt",
    "/soh/assets/extractor/filelists/gamecube_pal.txt",
    "/soh/assets/extractor/filelists/dbg.txt",
    nullptr
};

// CRC -> file list path mapping
static const char* GetFileListForCrc(uint32_t crc) {
    // Known CRCs from Extract.cpp
    switch (crc) {
        case 0xF3DD35BA: // OOT_NTSC_US_GC
        case 0xF611F4BA: // OOT_NTSC_JP_GC
        case 0xF7F52DB8: // OOT_NTSC_JP_GC_CE
        case 0xF034001A: // OOT_NTSC_US_MQ
        case 0xF43B45BA: // OOT_NTSC_JP_MQ
            return "/soh/assets/extractor/filelists/gamecube.txt";
        case 0x09465AC3: // OOT_PAL_GC
        case 0x1D4136F3: // OOT_PAL_MQ
            return "/soh/assets/extractor/filelists/gamecube_pal.txt";
        case 0xEC7011B7: // OOT_NTSC_10
        case 0xD43DA81F: // OOT_NTSC_11
            return "/soh/assets/extractor/filelists/ntsc_oot.txt";
        case 0x693BA2AE: // OOT_NTSC_12
            return "/soh/assets/extractor/filelists/ntsc_12_oot.txt";
        case 0xB044B569: // OOT_PAL_10
        case 0xB2055FBD: // OOT_PAL_11
            return "/soh/assets/extractor/filelists/pal_oot.txt";
        case 0x871E1C92: // OOT_PAL_GC_DBG1
        case 0x87121EFE: // OOT_PAL_GC_DBG2
        case 0x917D18F6: // OOT_PAL_GC_MQ_DBG
            return "/soh/assets/extractor/filelists/dbg.txt";
        default:
            return nullptr;
    }
}

RomArchive::RomArchive(const std::string& path, std::vector<uint8_t>&& romData)
    : Archive(path), mRomData(std::move(romData)) {
}

RomArchive::~RomArchive() {
    Close();
}

bool RomArchive::Open() {
    mDma = std::make_unique<RomDma>(mRomData.data(), mRomData.size());

    if (!mDma->ParseDmaTable()) {
        fprintf(stderr, "[RomArchive] Failed to parse DMA table\n");
        return false;
    }

    if (!LoadFileList()) {
        fprintf(stderr, "[RomArchive] Failed to load file list — indexing by DMA index only\n");
        // Still usable, just can't map paths to entries
    }

    BuildFileIndex();
    return true;
}

bool RomArchive::Close() {
    mDma.reset();
    mDmaCache.clear();
    mPathToDmaIndex.clear();
    return true;
}

bool RomArchive::WriteFile(const std::string& filename, const std::vector<uint8_t>& data) {
    // ROM archive is read-only
    return false;
}

bool RomArchive::LoadFileList() {
    uint32_t crc = mDma->GetRomCrc();
    const char* fileListPath = GetFileListForCrc(crc);

    if (!fileListPath) {
        fprintf(stderr, "[RomArchive] No file list known for ROM CRC 0x%08X\n", crc);
        return false;
    }

    printf("[RomArchive] Loading file list: %s\n", fileListPath);

    std::ifstream file(fileListPath);
    if (!file.is_open()) {
        fprintf(stderr, "[RomArchive] Cannot open file list: %s\n", fileListPath);
        return false;
    }

    std::string line;
    size_t index = 0;
    while (std::getline(file, line)) {
        // Trim whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) {
            index++;
            continue;
        }

        if (index < mDma->GetEntryCount()) {
            mPathToDmaIndex[line] = index;
        }
        index++;
    }

    printf("[RomArchive] Mapped %zu file paths to DMA entries\n", mPathToDmaIndex.size());
    return true;
}

void RomArchive::BuildFileIndex() {
    // Index all known file paths so the ArchiveManager can find them
    for (const auto& [path, dmaIndex] : mPathToDmaIndex) {
        IndexFile(path);
    }

    // Also add a "version" file so Archive::Load() can read it
    // We'll handle this specially in LoadFile
    IndexFile("version");
}

std::shared_ptr<Ship::File> RomArchive::LoadFile(uint64_t hash) {
    // Delegate to base class which resolves hash -> path -> LoadFile(string)
    const std::string* filePath =
        Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager()->HashToString(hash);
    if (filePath == nullptr) {
        return nullptr;
    }
    return LoadFile(*filePath);
}

std::shared_ptr<Ship::File> RomArchive::LoadFile(const std::string& filePath) {
    // Handle the special "version" file that Archive::Load() expects
    if (filePath == "version") {
        auto file = std::make_shared<Ship::File>();
        // Create a minimal version buffer:
        // byte 0: endianness (0 = big, 1 = little)
        // bytes 1-4: game version CRC (uint32_t)
        auto buffer = std::make_shared<std::vector<char>>(5);
        (*buffer)[0] = 1; // Little endian
        uint32_t crc = mDma->GetRomCrc();
        memcpy(buffer->data() + 1, &crc, 4);
        file->Buffer = buffer;
        file->IsLoaded = true;
        return file;
    }

    auto it = mPathToDmaIndex.find(filePath);
    if (it == mPathToDmaIndex.end()) {
        return nullptr;
    }

    size_t dmaIndex = it->second;

    // Check cache first
    auto cacheIt = mDmaCache.find(dmaIndex);
    if (cacheIt == mDmaCache.end()) {
        // Load and decompress from ROM
        auto data = mDma->LoadEntry(dmaIndex);
        if (data.empty()) {
            return nullptr;
        }
        cacheIt = mDmaCache.emplace(dmaIndex, std::move(data)).first;
    }

    const auto& data = cacheIt->second;

    auto file = std::make_shared<Ship::File>();
    auto buffer = std::make_shared<std::vector<char>>(data.size());
    memcpy(buffer->data(), data.data(), data.size());
    file->Buffer = buffer;
    file->IsLoaded = true;

    return file;
}

bool RomArchive::IsMasterQuest() const {
    return mDma && mDma->IsMasterQuest();
}

size_t RomArchive::FindDmaIndexByName(const std::string& name) const {
    auto it = mPathToDmaIndex.find(name);
    if (it != mPathToDmaIndex.end()) {
        return it->second;
    }
    return SIZE_MAX;
}

#endif // __EMSCRIPTEN__
