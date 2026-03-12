#if defined(__EMSCRIPTEN__) && defined(ROM_DIRECT_LOADING)

#include "RomDirectAudio.h"
#include "RomArchive.h"
#include "web_main.h"

#include <cstdio>
#include <cstring>
#include <vector>

// The ROM contains three main audio DMA entries:
//   - Audiobank: sound font definitions (table header + font binary data)
//   - Audioseq:  sequence data (table header + sequence binary blobs)
//   - Audiotable: raw ADPCM sample data (table header + sample data)
//
// Each starts with an AudioTable header:
//   struct AudioTable {
//       s16 numEntries;
//       s16 unkMediumParam;
//       u32 romAddr;        // base ROM address (unused when in memory)
//       u8  pad[8];
//       AudioTableEntry entries[];  // numEntries entries, each 16 bytes
//   };
//   struct AudioTableEntry {
//       u32 romAddr;        // offset from start of data section
//       u32 size;
//       s8  medium;
//       s8  cachePolicy;
//       s16 shortData1;
//       s16 shortData2;
//       s16 shortData3;
//   };

// DMA entry names in the file list
static constexpr const char* kAudiobankName  = "Audiobank";
static constexpr const char* kAudioseqName   = "Audioseq";
static constexpr const char* kAudiotableName = "Audiotable";

// Parsed AudioTable header (matches z64audio.h layout)
struct RomAudioTableEntry {
    uint32_t romAddr;   // offset within the data section (after the header)
    uint32_t size;
    int8_t   medium;
    int8_t   cachePolicy;
    int16_t  shortData1;
    int16_t  shortData2;
    int16_t  shortData3;
};

struct RomAudioTable {
    uint16_t numEntries;
    uint16_t unkMediumParam;
    std::vector<RomAudioTableEntry> entries;
    // Pointer to the data section (after the header) within the DMA blob
    const uint8_t* dataBase;
    size_t dataSize;
};

static bool sInitialized = false;

// In-memory copies of the three audio DMA blobs
static std::vector<uint8_t> sAudiobankData;
static std::vector<uint8_t> sAudioseqData;
static std::vector<uint8_t> sAudiotableData;

// Parsed table headers
static RomAudioTable sAudiobankTable;
static RomAudioTable sAudioseqTable;
static RomAudioTable sAudiotableTable;

// Read a big-endian uint32 from a buffer
static uint32_t ReadBE32(const uint8_t* buf) {
    return (uint32_t(buf[0]) << 24) | (uint32_t(buf[1]) << 16) |
           (uint32_t(buf[2]) << 8)  | uint32_t(buf[3]);
}

static uint16_t ReadBE16(const uint8_t* buf) {
    return (uint16_t(buf[0]) << 8) | uint16_t(buf[1]);
}

static int16_t ReadBE16s(const uint8_t* buf) {
    return (int16_t)ReadBE16(buf);
}

// Parse an AudioTable header from raw DMA data.
// The header starts at the beginning of the blob:
//   offset 0x00: numEntries (BE s16)
//   offset 0x02: unkMediumParam (BE s16)
//   offset 0x04: romAddr (BE u32, unused)
//   offset 0x08: pad[8]
//   offset 0x10: entries[numEntries], each 16 bytes
// Data section starts right after the last entry.
static bool ParseAudioTable(const std::vector<uint8_t>& blob, RomAudioTable& table) {
    if (blob.size() < 0x10) {
        return false;
    }

    table.numEntries = ReadBE16(blob.data());
    table.unkMediumParam = ReadBE16(blob.data() + 2);

    size_t headerSize = 0x10 + (size_t)table.numEntries * 0x10;
    if (blob.size() < headerSize) {
        fprintf(stderr, "[RomDirectAudio] Blob too small for %u entries (need %zu, have %zu)\n",
                table.numEntries, headerSize, blob.size());
        return false;
    }

    table.entries.resize(table.numEntries);
    for (uint16_t i = 0; i < table.numEntries; i++) {
        const uint8_t* e = blob.data() + 0x10 + i * 0x10;
        table.entries[i].romAddr     = ReadBE32(e + 0x00);
        table.entries[i].size        = ReadBE32(e + 0x04);
        table.entries[i].medium      = (int8_t)e[0x08];
        table.entries[i].cachePolicy = (int8_t)e[0x09];
        table.entries[i].shortData1  = ReadBE16s(e + 0x0A);
        table.entries[i].shortData2  = ReadBE16s(e + 0x0C);
        table.entries[i].shortData3  = ReadBE16s(e + 0x0E);
    }

    // Data section starts after the header
    table.dataBase = blob.data() + headerSize;
    table.dataSize = blob.size() - headerSize;

    return true;
}

extern "C" int32_t RomDirectAudio_Init(void) {
    auto romArchive = web_get_rom_archive();
    if (!romArchive || !romArchive->GetDma()) {
        fprintf(stderr, "[RomDirectAudio] No ROM archive available\n");
        return -1;
    }

    // Load the three audio DMA entries by name
    size_t audiobankIdx = romArchive->FindDmaIndexByName(kAudiobankName);
    size_t audioseqIdx  = romArchive->FindDmaIndexByName(kAudioseqName);
    size_t audiotableIdx = romArchive->FindDmaIndexByName(kAudiotableName);

    if (audiobankIdx == SIZE_MAX || audioseqIdx == SIZE_MAX || audiotableIdx == SIZE_MAX) {
        fprintf(stderr, "[RomDirectAudio] Could not find audio DMA entries "
                "(bank=%zu, seq=%zu, table=%zu)\n",
                audiobankIdx, audioseqIdx, audiotableIdx);
        return -1;
    }

    RomDma* dma = romArchive->GetDma();

    sAudiobankData  = dma->LoadEntry(audiobankIdx);
    sAudioseqData   = dma->LoadEntry(audioseqIdx);
    sAudiotableData = dma->LoadEntry(audiotableIdx);

    if (sAudiobankData.empty() || sAudioseqData.empty() || sAudiotableData.empty()) {
        fprintf(stderr, "[RomDirectAudio] Failed to load audio DMA entries\n");
        return -1;
    }

    printf("[RomDirectAudio] Loaded audio blobs: Audiobank=%zuKB, Audioseq=%zuKB, Audiotable=%zuKB\n",
           sAudiobankData.size() / 1024, sAudioseqData.size() / 1024, sAudiotableData.size() / 1024);

    // Parse table headers
    if (!ParseAudioTable(sAudiobankData, sAudiobankTable)) {
        fprintf(stderr, "[RomDirectAudio] Failed to parse Audiobank table\n");
        return -1;
    }
    if (!ParseAudioTable(sAudioseqData, sAudioseqTable)) {
        fprintf(stderr, "[RomDirectAudio] Failed to parse Audioseq table\n");
        return -1;
    }
    if (!ParseAudioTable(sAudiotableData, sAudiotableTable)) {
        fprintf(stderr, "[RomDirectAudio] Failed to parse Audiotable table\n");
        return -1;
    }

    printf("[RomDirectAudio] Audio tables: %u fonts, %u sequences, %u sample banks\n",
           sAudiobankTable.numEntries, sAudioseqTable.numEntries, sAudiotableTable.numEntries);

    sInitialized = true;
    return 0;
}

extern "C" int32_t RomDirectAudio_GetSequenceData(uint32_t seqId,
                                                   const uint8_t** outData,
                                                   uint32_t* outSize) {
    if (!sInitialized || seqId >= sAudioseqTable.entries.size()) {
        return -1;
    }

    const auto& entry = sAudioseqTable.entries[seqId];
    if (entry.size == 0) {
        return -1;
    }

    // entry.romAddr is offset within the data section
    if (entry.romAddr + entry.size > sAudioseqData.size()) {
        fprintf(stderr, "[RomDirectAudio] Sequence %u data out of bounds\n", seqId);
        return -1;
    }

    // The romAddr in the table entries is relative to the start of the blob
    // (including header), not the data section. So we use the blob directly.
    // Actually, in N64 ROM format, entry.romAddr is an offset from the
    // start of the entire Audioseq segment (including header).
    *outData = sAudioseqData.data() + entry.romAddr;
    *outSize = entry.size;
    return 0;
}

extern "C" uint32_t RomDirectAudio_GetSequenceCount(void) {
    return sInitialized ? sAudioseqTable.numEntries : 0;
}

extern "C" uint32_t RomDirectAudio_GetFontCount(void) {
    return sInitialized ? sAudiobankTable.numEntries : 0;
}

extern "C" uint32_t RomDirectAudio_GetSampleBankCount(void) {
    return sInitialized ? sAudiotableTable.numEntries : 0;
}

extern "C" int32_t RomDirectAudio_IsInitialized(void) {
    return sInitialized ? 1 : 0;
}

#endif // __EMSCRIPTEN__ && ROM_DIRECT_LOADING
