#pragma once

#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

#include "ship/resource/archive/Archive.h"
#include "RomDma.h"

class RomArchive final : virtual public Ship::Archive {
  public:
    RomArchive(const std::string& path, std::vector<uint8_t>&& romData);
    ~RomArchive();

    bool Open() override;
    bool Close() override;
    bool WriteFile(const std::string& filename, const std::vector<uint8_t>& data) override;

    std::shared_ptr<Ship::File> LoadFile(const std::string& filePath) override;
    std::shared_ptr<Ship::File> LoadFile(uint64_t hash) override;

    RomDma* GetDma() { return mDma.get(); }
    bool IsMasterQuest() const;

    // Look up a DMA index by file name (as it appears in the file list)
    size_t FindDmaIndexByName(const std::string& name) const;

  private:
    void BuildFileIndex();
    bool LoadFileList();

    std::vector<uint8_t> mRomData;
    std::unique_ptr<RomDma> mDma;

    // Map from file path (e.g. "ydan_scene") to DMA index
    std::unordered_map<std::string, size_t> mPathToDmaIndex;

    // Cache of decompressed DMA entries
    mutable std::unordered_map<size_t, std::vector<uint8_t>> mDmaCache;
};

#endif // __EMSCRIPTEN__
