#pragma once
#include "utils.h"
#include "ntfs_parser.h"
#include <vector>
#include <string>
#include <functional>

namespace rt {

using RecoveryProgressCallback = std::function<void(uint32_t current, uint32_t total,
                                                     const std::wstring& fileName)>;

class Recovery {
public:
    // Recover selected files to the destination directory
    static bool RecoverFiles(const std::vector<RecoveredFile>& files,
                             const std::wstring& sourceDrive,
                             const std::wstring& destDir,
                             RecoveryProgressCallback progressCb = nullptr);

    // Recover a single file
    static bool RecoverFile(const RecoveredFile& file,
                            const std::wstring& sourceDrive,
                            const std::wstring& destPath);

private:
    // Recover from MFT data runs
    static bool RecoverFromMft(const RecoveredFile& file,
                               const std::wstring& sourceDrive,
                               const std::wstring& destPath);

    // Recover by carving (using disk offset + signature)
    static bool RecoverByCarving(const RecoveredFile& file,
                                 const std::wstring& sourceDrive,
                                 const std::wstring& destPath);

    // Find the actual size of a carved file using footer signature
    static uint64_t FindActualSize(HANDLE hDisk, uint64_t startOffset,
                                   const std::vector<uint8_t>& footer,
                                   uint64_t maxSize);
};

} // namespace rt
