#pragma once

#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>

struct RomDmaEntry {
    uint32_t virtStart;
    uint32_t virtEnd;
    uint32_t physStart;
    uint32_t physEnd;
};

class RomDma {
  public:
    RomDma(const uint8_t* romData, size_t romSize);

    bool ParseDmaTable();
    std::vector<uint8_t> LoadEntry(size_t index) const;
    std::vector<uint8_t> LoadVrom(uint32_t vromStart, uint32_t vromEnd) const;
    size_t FindEntry(uint32_t vromStart) const;
    size_t GetEntryCount() const { return mDmaTable.size(); }
    const RomDmaEntry& GetEntry(size_t index) const { return mDmaTable[index]; }
    uint32_t GetRomCrc() const;
    bool IsMasterQuest() const;

    static std::vector<uint8_t> Yaz0Decompress(const uint8_t* src, size_t srcSize);

  private:
    uint32_t ReadBE32(size_t offset) const;
    uint32_t FindDmaTableOffset() const;

    const uint8_t* mRomData;
    size_t mRomSize;
    std::vector<RomDmaEntry> mDmaTable;
    std::unordered_map<uint32_t, size_t> mVromToIndex;
};

#endif // __EMSCRIPTEN__
