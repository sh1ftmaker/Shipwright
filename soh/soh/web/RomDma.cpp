#ifdef __EMSCRIPTEN__

#include "RomDma.h"
#include <cstring>
#include <cstdio>

// Known ROM version CRCs (from Extract.cpp) — at ROM offset 0x10 (big-endian)
static constexpr uint32_t OOT_PAL_GC = 0x09465AC3;
static constexpr uint32_t OOT_PAL_MQ = 0x1D4136F3;
static constexpr uint32_t OOT_PAL_GC_DBG1 = 0x871E1C92;
static constexpr uint32_t OOT_PAL_GC_DBG2 = 0x87121EFE;
static constexpr uint32_t OOT_PAL_GC_MQ_DBG = 0x917D18F6;
static constexpr uint32_t OOT_PAL_10 = 0xB044B569;
static constexpr uint32_t OOT_PAL_11 = 0xB2055FBD;
static constexpr uint32_t OOT_NTSC_US_GC = 0xF3DD35BA;
static constexpr uint32_t OOT_NTSC_JP_GC = 0xF611F4BA;
static constexpr uint32_t OOT_NTSC_JP_GC_CE = 0xF7F52DB8;
static constexpr uint32_t OOT_NTSC_US_MQ = 0xF034001A;
static constexpr uint32_t OOT_NTSC_JP_MQ = 0xF43B45BA;
static constexpr uint32_t OOT_NTSC_10 = 0xEC7011B7;
static constexpr uint32_t OOT_NTSC_11 = 0xD43DA81F;
static constexpr uint32_t OOT_NTSC_12 = 0x693BA2AE;

RomDma::RomDma(const uint8_t* romData, size_t romSize)
    : mRomData(romData), mRomSize(romSize) {
}

uint32_t RomDma::ReadBE32(size_t offset) const {
    if (offset + 4 > mRomSize) return 0;
    return (uint32_t(mRomData[offset]) << 24) |
           (uint32_t(mRomData[offset + 1]) << 16) |
           (uint32_t(mRomData[offset + 2]) << 8) |
           uint32_t(mRomData[offset + 3]);
}

uint32_t RomDma::GetRomCrc() const {
    // ROM CRC is at offset 0x10 in big-endian N64 format
    return ReadBE32(0x10);
}

bool RomDma::IsMasterQuest() const {
    uint32_t crc = GetRomCrc();
    return crc == OOT_PAL_MQ ||
           crc == OOT_PAL_GC_MQ_DBG ||
           crc == OOT_NTSC_US_MQ ||
           crc == OOT_NTSC_JP_MQ;
}

uint32_t RomDma::FindDmaTableOffset() const {
    // The dmadata file is always the 3rd entry in itself (index 2).
    // We can find it by scanning for the characteristic pattern:
    // The DMA table starts with entries for makerom, boot, dmadata.
    // Entry 0 (makerom): virtStart=0, virtEnd=0x1060, physStart=0, physEnd=0
    // The first entry always has virtStart=0.
    //
    // Strategy: search for the string "dmadata" in the ROM, or use known
    // offsets per ROM version. Using known offsets is more reliable.

    uint32_t crc = GetRomCrc();

    // Known DMA table offsets per ROM version
    // These are the physical ROM offsets where the DMA table starts
    switch (crc) {
        case OOT_NTSC_10:       return 0x7430;
        case OOT_NTSC_11:       return 0x7430;
        case OOT_NTSC_12:       return 0x7960;
        case OOT_PAL_10:        return 0x7950;
        case OOT_PAL_11:        return 0x7950;
        case OOT_NTSC_US_GC:    return 0x7170;
        case OOT_NTSC_JP_GC:    return 0x7170;
        case OOT_NTSC_JP_GC_CE: return 0x7170;
        case OOT_PAL_GC:        return 0x7170;
        case OOT_NTSC_US_MQ:    return 0x7170;
        case OOT_NTSC_JP_MQ:    return 0x7170;
        case OOT_PAL_MQ:        return 0x7170;
        case OOT_PAL_GC_DBG1:   return 0x12F70;
        case OOT_PAL_GC_DBG2:   return 0x12F70;
        case OOT_PAL_GC_MQ_DBG: return 0x12F70;
        default: break;
    }

    // Fallback: scan for DMA table pattern.
    // The DMA table entry for itself (index 2) points back to the table.
    // Entry format: virtStart, virtEnd, physStart, physEnd (each 4 bytes BE)
    // Entry 0 always has virtStart=0.
    // We scan for a location where entry[0].virtStart == 0 and entry[2].physStart
    // equals the current scan position.
    for (size_t offset = 0x1000; offset < mRomSize - 0x30; offset += 0x10) {
        uint32_t e0_vstart = ReadBE32(offset);
        if (e0_vstart != 0) continue;

        uint32_t e0_vend = ReadBE32(offset + 0x04);
        if (e0_vend < 0x1000 || e0_vend > 0x2000) continue;

        // Check entry 2 (dmadata itself) — its physStart should equal this offset
        uint32_t e2_physStart = ReadBE32(offset + 0x28);
        if (e2_physStart == offset) {
            return offset;
        }
    }

    return 0;
}

bool RomDma::ParseDmaTable() {
    uint32_t dmaOffset = FindDmaTableOffset();
    if (dmaOffset == 0) {
        fprintf(stderr, "[RomDma] Failed to find DMA table in ROM\n");
        return false;
    }

    printf("[RomDma] DMA table found at ROM offset 0x%X\n", dmaOffset);

    mDmaTable.clear();
    mVromToIndex.clear();

    // Read DMA entries until we hit an all-zero entry
    for (size_t i = 0; dmaOffset + (i * 16) + 16 <= mRomSize; i++) {
        size_t entryOffset = dmaOffset + (i * 16);
        RomDmaEntry entry;
        entry.virtStart = ReadBE32(entryOffset + 0x00);
        entry.virtEnd   = ReadBE32(entryOffset + 0x04);
        entry.physStart = ReadBE32(entryOffset + 0x08);
        entry.physEnd   = ReadBE32(entryOffset + 0x0C);

        // End of table: all zeros (but skip entry 0 which has virtStart=0)
        if (i > 0 && entry.virtStart == 0 && entry.virtEnd == 0 &&
            entry.physStart == 0 && entry.physEnd == 0) {
            break;
        }

        mDmaTable.push_back(entry);
        mVromToIndex[entry.virtStart] = i;
    }

    printf("[RomDma] Parsed %zu DMA table entries\n", mDmaTable.size());
    return !mDmaTable.empty();
}

size_t RomDma::FindEntry(uint32_t vromStart) const {
    auto it = mVromToIndex.find(vromStart);
    if (it != mVromToIndex.end()) {
        return it->second;
    }
    return SIZE_MAX;
}

std::vector<uint8_t> RomDma::LoadEntry(size_t index) const {
    if (index >= mDmaTable.size()) {
        return {};
    }

    const auto& entry = mDmaTable[index];
    uint32_t virtSize = entry.virtEnd - entry.virtStart;

    if (virtSize == 0) {
        return {};
    }

    // physEnd == 0 means uncompressed, physEnd != 0 means Yaz0 compressed
    if (entry.physEnd == 0) {
        // Uncompressed: copy directly from ROM
        if (entry.physStart + virtSize > mRomSize) {
            fprintf(stderr, "[RomDma] Entry %zu: ROM read out of bounds (phys=0x%X, size=0x%X)\n",
                    index, entry.physStart, virtSize);
            return {};
        }
        return std::vector<uint8_t>(
            mRomData + entry.physStart,
            mRomData + entry.physStart + virtSize
        );
    } else {
        // Compressed: decompress with Yaz0
        uint32_t compSize = entry.physEnd - entry.physStart;
        if (entry.physStart + compSize > mRomSize) {
            fprintf(stderr, "[RomDma] Entry %zu: Compressed ROM read out of bounds\n", index);
            return {};
        }
        return Yaz0Decompress(mRomData + entry.physStart, compSize);
    }
}

std::vector<uint8_t> RomDma::LoadVrom(uint32_t vromStart, uint32_t vromEnd) const {
    size_t index = FindEntry(vromStart);
    if (index == SIZE_MAX) {
        fprintf(stderr, "[RomDma] No DMA entry found for VROM 0x%X\n", vromStart);
        return {};
    }
    return LoadEntry(index);
}

// Standalone Yaz0 decompression (does not depend on ZAPDTR)
std::vector<uint8_t> RomDma::Yaz0Decompress(const uint8_t* src, size_t srcSize) {
    if (srcSize < 0x10) return {};

    // Verify Yaz0 magic
    if (src[0] != 'Y' || src[1] != 'a' || src[2] != 'z' || src[3] != '0') {
        // Not compressed — return as-is
        return std::vector<uint8_t>(src, src + srcSize);
    }

    // Read decompressed size (big-endian at offset 4)
    uint32_t decompSize = (uint32_t(src[4]) << 24) | (uint32_t(src[5]) << 16) |
                          (uint32_t(src[6]) << 8) | uint32_t(src[7]);

    std::vector<uint8_t> dest(decompSize);
    uint32_t srcPos = 0x10; // Skip header
    uint32_t dstPos = 0;
    uint8_t codeByte = 0;
    uint8_t bitCount = 0;

    while (dstPos < decompSize && srcPos < srcSize) {
        if (bitCount == 0) {
            codeByte = src[srcPos++];
            bitCount = 8;
        }

        if (codeByte & 0x80) {
            // Direct copy
            if (srcPos >= srcSize) break;
            dest[dstPos++] = src[srcPos++];
        } else {
            // Reference copy
            if (srcPos + 1 >= srcSize) break;
            uint8_t byte1 = src[srcPos++];
            uint8_t byte2 = src[srcPos++];

            uint32_t dist = ((byte1 & 0x0F) << 8) | byte2;
            uint32_t copyPos = dstPos - (dist + 1);
            uint32_t numBytes = byte1 >> 4;

            if (numBytes == 0) {
                if (srcPos >= srcSize) break;
                numBytes = src[srcPos++] + 0x12;
            } else {
                numBytes += 2;
            }

            for (uint32_t i = 0; i < numBytes && dstPos < decompSize; i++) {
                dest[dstPos++] = dest[copyPos++];
            }
        }

        codeByte <<= 1;
        bitCount--;
    }

    return dest;
}

#endif // __EMSCRIPTEN__
