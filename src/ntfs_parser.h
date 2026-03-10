#pragma once
#include "utils.h"
#include <vector>
#include <cstdint>

namespace rt {

// ── NTFS structures (packed) ────────────────────────────────────────────────
#pragma pack(push, 1)

struct NtfsBootSector {
    uint8_t  jump[3];
    char     oemId[8];             // "NTFS    "
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint8_t  reserved1[7];
    uint8_t  mediaDescriptor;
    uint8_t  reserved2[2];
    uint16_t sectorsPerTrack;
    uint16_t numberOfHeads;
    uint32_t hiddenSectors;
    uint8_t  reserved3[8];
    uint64_t totalSectors;
    uint64_t mftClusterNumber;     // LCN of $MFT
    uint64_t mftMirrorCluster;
    int8_t   clustersPerMftRecord; // if negative, size = 2^|value|
    uint8_t  reserved4[3];
    int8_t   clustersPerIndexBlock;
    uint8_t  reserved5[3];
    uint64_t volumeSerialNumber;
};

struct MftRecordHeader {
    char     signature[4];     // "FILE"
    uint16_t updateSeqOffset;
    uint16_t updateSeqSize;
    uint64_t logFileSeqNum;
    uint16_t sequenceNumber;
    uint16_t hardLinkCount;
    uint16_t firstAttributeOffset;
    uint16_t flags;            // 0x01 = in use, 0x02 = directory
    uint32_t usedSize;
    uint32_t allocatedSize;
    uint64_t baseRecordRef;
    uint16_t nextAttributeId;
};

struct MftAttributeHeader {
    uint32_t type;
    uint32_t length;
    uint8_t  nonResident;
    uint8_t  nameLength;
    uint16_t nameOffset;
    uint16_t flags;
    uint16_t attributeId;
};

struct ResidentAttrData {
    uint32_t valueLength;
    uint16_t valueOffset;
    uint16_t indexedFlag;
};

struct NonResidentAttrData {
    uint64_t startVcn;
    uint64_t lastVcn;
    uint16_t dataRunsOffset;
    uint16_t compressionUnitSize;
    uint32_t padding;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint64_t initializedSize;
};

// $FILE_NAME attribute (0x30)
struct FileNameAttribute {
    uint64_t parentDirRef;
    FILETIME creationTime;
    FILETIME modificationTime;
    FILETIME mftModifiedTime;
    FILETIME readTime;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint32_t flags;
    uint32_t reparseValue;
    uint8_t  nameLength;       // in characters
    uint8_t  nameNamespace;    // 0=POSIX, 1=Win32, 2=DOS, 3=Win32+DOS
    wchar_t  name[1];         // variable length
};

#pragma pack(pop)

// NTFS attribute type constants
constexpr uint32_t ATTR_STANDARD_INFO = 0x10;
constexpr uint32_t ATTR_FILE_NAME     = 0x30;
constexpr uint32_t ATTR_DATA          = 0x80;
constexpr uint32_t ATTR_END_MARKER    = 0xFFFFFFFF;

// MFT record flags
constexpr uint16_t MFT_RECORD_IN_USE    = 0x0001;
constexpr uint16_t MFT_RECORD_DIRECTORY  = 0x0002;

// ── NTFS parser class ───────────────────────────────────────────────────────
class NtfsParser {
public:
    NtfsParser() = default;
    ~NtfsParser();

    // Open a volume (e.g., "\\\\.\\C:")
    bool Open(const std::wstring& volumePath);
    void Close();

    // Parse MFT and collect deleted file records
    bool ScanDeletedFiles(std::vector<RecoveredFile>& results,
                          ProgressCallback progressCb = nullptr);

    // Read clusters for a given data run
    bool ReadClusters(uint64_t lcn, uint32_t count, std::vector<uint8_t>& buffer);

    uint32_t GetBytesPerCluster() const { return bytesPerCluster_; }
    uint32_t GetMftRecordSize()   const { return mftRecordSize_; }

private:
    bool ReadBootSector();
    bool ParseMftRecord(const uint8_t* record, uint64_t recordIndex,
                        RecoveredFile& outFile);
    bool ApplyFixups(uint8_t* record, uint32_t recordSize);

    HANDLE   hVolume_        = INVALID_HANDLE_VALUE;
    uint32_t bytesPerSector_  = 512;
    uint32_t sectorsPerCluster_ = 8;
    uint32_t bytesPerCluster_ = 4096;
    uint32_t mftRecordSize_   = 1024;
    uint64_t mftStartLcn_     = 0;
    uint64_t totalSectors_    = 0;
    std::wstring volumePath_;
};

} // namespace rt
