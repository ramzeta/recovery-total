#include "ntfs_parser.h"
#include <algorithm>
#include <cstring>

namespace rt {

NtfsParser::~NtfsParser() {
    Close();
}

bool NtfsParser::Open(const std::wstring& volumePath) {
    Close();
    volumePath_ = volumePath;

    // Open volume for raw read
    hVolume_ = CreateFileW(volumePath.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING,
                           FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
                           nullptr);
    if (hVolume_ == INVALID_HANDLE_VALUE) return false;

    return ReadBootSector();
}

void NtfsParser::Close() {
    if (hVolume_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hVolume_);
        hVolume_ = INVALID_HANDLE_VALUE;
    }
}

bool NtfsParser::ReadBootSector() {
    // Read first sector
    std::vector<uint8_t> sector(512);
    DWORD bytesRead = 0;
    LARGE_INTEGER pos = {};
    SetFilePointerEx(hVolume_, pos, nullptr, FILE_BEGIN);

    if (!ReadFile(hVolume_, sector.data(), 512, &bytesRead, nullptr) || bytesRead < 512)
        return false;

    auto* bs = reinterpret_cast<NtfsBootSector*>(sector.data());

    // Validate NTFS signature
    if (std::string(bs->oemId, 4) != "NTFS")
        return false;

    bytesPerSector_    = bs->bytesPerSector;
    sectorsPerCluster_ = bs->sectorsPerCluster;
    bytesPerCluster_   = bytesPerSector_ * sectorsPerCluster_;
    mftStartLcn_       = bs->mftClusterNumber;
    totalSectors_      = bs->totalSectors;

    // Calculate MFT record size
    if (bs->clustersPerMftRecord >= 0) {
        mftRecordSize_ = bs->clustersPerMftRecord * bytesPerCluster_;
    } else {
        mftRecordSize_ = 1U << static_cast<uint32_t>(-bs->clustersPerMftRecord);
    }

    return true;
}

bool NtfsParser::ReadClusters(uint64_t lcn, uint32_t count, std::vector<uint8_t>& buffer) {
    uint64_t offset = lcn * bytesPerCluster_;
    uint32_t size   = count * bytesPerCluster_;
    buffer.resize(size);

    LARGE_INTEGER pos;
    pos.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(hVolume_, pos, nullptr, FILE_BEGIN))
        return false;

    DWORD bytesRead = 0;
    if (!ReadFile(hVolume_, buffer.data(), size, &bytesRead, nullptr))
        return false;

    return bytesRead == size;
}

bool NtfsParser::ApplyFixups(uint8_t* record, uint32_t recordSize) {
    auto* header = reinterpret_cast<MftRecordHeader*>(record);

    uint16_t fixupOffset = header->updateSeqOffset;
    uint16_t fixupCount  = header->updateSeqSize;

    if (fixupCount == 0 || fixupOffset + fixupCount * 2 > recordSize)
        return false;

    uint16_t* fixupArray = reinterpret_cast<uint16_t*>(record + fixupOffset);
    uint16_t  signature  = fixupArray[0];

    // Apply fixups at end of each sector
    for (uint16_t i = 1; i < fixupCount; ++i) {
        uint32_t sectorEnd = i * bytesPerSector_ - 2;
        if (sectorEnd + 1 >= recordSize) break;

        uint16_t* target = reinterpret_cast<uint16_t*>(record + sectorEnd);
        if (*target != signature) return false;  // corruption
        *target = fixupArray[i];
    }

    return true;
}

bool NtfsParser::ParseMftRecord(const uint8_t* record, uint64_t recordIndex,
                                RecoveredFile& outFile) {
    auto* header = reinterpret_cast<const MftRecordHeader*>(record);

    // We want deleted file records (not in use, not a directory)
    bool inUse    = (header->flags & MFT_RECORD_IN_USE) != 0;
    bool isDir    = (header->flags & MFT_RECORD_DIRECTORY) != 0;

    if (inUse || isDir) return false;

    // Walk attributes
    uint32_t offset = header->firstAttributeOffset;
    bool foundName = false;

    while (offset + sizeof(MftAttributeHeader) < mftRecordSize_) {
        auto* attr = reinterpret_cast<const MftAttributeHeader*>(record + offset);

        if (attr->type == ATTR_END_MARKER || attr->length == 0)
            break;

        if (offset + attr->length > mftRecordSize_)
            break;

        // $FILE_NAME attribute
        if (attr->type == ATTR_FILE_NAME && !attr->nonResident) {
            auto* resData = reinterpret_cast<const ResidentAttrData*>(
                record + offset + sizeof(MftAttributeHeader));
            uint32_t valueOff = offset + sizeof(MftAttributeHeader) + resData->valueOffset;

            if (valueOff + sizeof(FileNameAttribute) <= mftRecordSize_) {
                auto* fn = reinterpret_cast<const FileNameAttribute*>(record + valueOff);

                // Skip DOS-only names (namespace 2)
                if (fn->nameNamespace != 2 && fn->nameLength > 0) {
                    outFile.fileName = std::wstring(fn->name, fn->nameLength);
                    outFile.extension = ExtensionFromName(outFile.fileName);
                    outFile.fileSize = fn->realSize;
                    outFile.creationTime = fn->creationTime;
                    outFile.modifiedTime = fn->modificationTime;
                    outFile.mftRecordIndex = recordIndex;
                    outFile.fromMFT = true;
                    foundName = true;
                }
            }
        }

        // $DATA attribute - check if data runs exist
        if (attr->type == ATTR_DATA && foundName) {
            if (attr->nonResident) {
                auto* nrData = reinterpret_cast<const NonResidentAttrData*>(
                    record + offset + sizeof(MftAttributeHeader));
                outFile.fileSize = nrData->realSize;

                // Parse first data run to get disk offset
                uint32_t drOffset = offset + sizeof(MftAttributeHeader) +
                                    nrData->dataRunsOffset;
                if (drOffset < mftRecordSize_) {
                    uint8_t drHeader = record[drOffset];
                    if (drHeader != 0) {
                        uint8_t lenSize = drHeader & 0x0F;
                        uint8_t offSize = (drHeader >> 4) & 0x0F;
                        if (drOffset + 1 + lenSize + offSize <= mftRecordSize_ && offSize > 0) {
                            int64_t lcn = 0;
                            memcpy(&lcn, record + drOffset + 1 + lenSize, offSize);
                            // Sign extend
                            if (record[drOffset + lenSize + offSize] & 0x80) {
                                for (uint8_t i = offSize; i < 8; ++i)
                                    lcn |= (0xFFLL << (i * 8));
                            }
                            outFile.diskOffset = static_cast<uint64_t>(lcn) * bytesPerCluster_;
                        }
                    }
                }

                // Assess recovery status
                if (outFile.diskOffset > 0 && outFile.fileSize > 0)
                    outFile.status = FileStatus::Good;
                else
                    outFile.status = FileStatus::Poor;
            } else {
                // Resident data - small file, excellent recovery chance
                outFile.status = FileStatus::Excellent;
            }
        }

        offset += attr->length;
    }

    // Filter out system/internal files
    if (!foundName) return false;
    if (outFile.fileName.empty() || outFile.fileName[0] == L'$') return false;
    if (outFile.fileSize == 0) return false;

    return true;
}

bool NtfsParser::ScanDeletedFiles(std::vector<RecoveredFile>& results,
                                  ProgressCallback progressCb) {
    if (hVolume_ == INVALID_HANDLE_VALUE) return false;

    // Read MFT region - we'll scan a reasonable number of records
    // Typical MFT has thousands of records; we scan up to ~256K records
    const uint64_t maxRecords = 262144;
    const uint32_t batchSize  = 64;  // records per read batch
    const uint32_t batchBytes = batchSize * mftRecordSize_;

    // Align batch to sector boundary
    uint32_t alignedBatchBytes = ((batchBytes + bytesPerSector_ - 1) / bytesPerSector_) * bytesPerSector_;

    std::vector<uint8_t> buffer(alignedBatchBytes);

    uint64_t mftByteOffset = mftStartLcn_ * bytesPerCluster_;
    ScanProgress progress;
    progress.totalBytes = maxRecords * mftRecordSize_;
    progress.currentAction = L"Scanning MFT for deleted files...";

    for (uint64_t recIdx = 0; recIdx < maxRecords; recIdx += batchSize) {
        uint64_t readOffset = mftByteOffset + recIdx * mftRecordSize_;

        LARGE_INTEGER pos;
        pos.QuadPart = static_cast<LONGLONG>(readOffset);
        if (!SetFilePointerEx(hVolume_, pos, nullptr, FILE_BEGIN))
            break;

        DWORD bytesRead = 0;
        if (!ReadFile(hVolume_, buffer.data(), alignedBatchBytes, &bytesRead, nullptr))
            break;

        if (bytesRead < mftRecordSize_) break;

        uint32_t recordsInBatch = bytesRead / mftRecordSize_;

        for (uint32_t i = 0; i < recordsInBatch; ++i) {
            uint8_t* recPtr = buffer.data() + i * mftRecordSize_;

            // Check "FILE" signature
            if (recPtr[0] != 'F' || recPtr[1] != 'I' || recPtr[2] != 'L' || recPtr[3] != 'E')
                continue;

            // Make a copy for fixup modification
            std::vector<uint8_t> recCopy(recPtr, recPtr + mftRecordSize_);
            if (!ApplyFixups(recCopy.data(), mftRecordSize_))
                continue;

            RecoveredFile file;
            if (ParseMftRecord(recCopy.data(), recIdx + i, file)) {
                results.push_back(std::move(file));
            }
        }

        // Progress
        progress.bytesScanned = (recIdx + recordsInBatch) * mftRecordSize_;
        progress.filesFound   = static_cast<uint32_t>(results.size());
        progress.progressPct  = 100.0 * progress.bytesScanned / progress.totalBytes;
        if (progressCb) progressCb(progress);
    }

    return true;
}

} // namespace rt
