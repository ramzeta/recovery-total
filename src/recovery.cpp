#include "recovery.h"
#include "file_signatures.h"
#include <algorithm>
#include <filesystem>

namespace rt {

bool Recovery::RecoverFiles(const std::vector<RecoveredFile>& files,
                            const std::wstring& sourceDrive,
                            const std::wstring& destDir,
                            RecoveryProgressCallback progressCb) {
    // Create destination directory
    std::filesystem::create_directories(destDir);

    // Create subdirectories by category
    auto& sigDb = GetSignatureDatabase();

    uint32_t total = static_cast<uint32_t>(files.size());
    uint32_t current = 0;
    uint32_t recovered = 0;

    for (auto& file : files) {
        if (file.status == FileStatus::Unrecoverable) {
            ++current;
            continue;
        }

        // Determine subdirectory
        std::wstring subDir = L"Other";
        if (file.signatureId >= 0) {
            auto* sig = GetSignatureById(file.signatureId);
            if (sig) subDir = sig->category;
        } else {
            // Guess from extension
            std::wstring ext = file.extension;
            if (ext == L".jpg" || ext == L".png" || ext == L".gif" ||
                ext == L".bmp" || ext == L".tif" || ext == L".webp")
                subDir = L"Image";
            else if (ext == L".pdf" || ext == L".doc" || ext == L".docx" ||
                     ext == L".xls" || ext == L".xlsx" || ext == L".ppt" || ext == L".pptx")
                subDir = L"Document";
            else if (ext == L".mp4" || ext == L".avi" || ext == L".mkv" || ext == L".mov")
                subDir = L"Video";
            else if (ext == L".mp3" || ext == L".wav" || ext == L".flac" || ext == L".ogg")
                subDir = L"Audio";
            else if (ext == L".zip" || ext == L".rar" || ext == L".7z" || ext == L".gz")
                subDir = L"Archive";
        }

        std::wstring targetDir = destDir + L"\\" + subDir;
        std::filesystem::create_directories(targetDir);

        // Ensure unique filename
        std::wstring targetPath = targetDir + L"\\" + file.fileName;
        int dupCount = 1;
        while (std::filesystem::exists(targetPath)) {
            auto stem = file.fileName.substr(0, file.fileName.rfind(L'.'));
            targetPath = targetDir + L"\\" + stem + L"_" +
                         std::to_wstring(++dupCount) + file.extension;
        }

        if (progressCb) progressCb(++current, total, file.fileName);

        if (RecoverFile(file, sourceDrive, targetPath))
            ++recovered;
    }

    return recovered > 0;
}

bool Recovery::RecoverFile(const RecoveredFile& file,
                           const std::wstring& sourceDrive,
                           const std::wstring& destPath) {
    if (file.fromMFT) {
        return RecoverFromMft(file, sourceDrive, destPath);
    } else {
        return RecoverByCarving(file, sourceDrive, destPath);
    }
}

bool Recovery::RecoverFromMft(const RecoveredFile& file,
                              const std::wstring& sourceDrive,
                              const std::wstring& destPath) {
    // Open volume
    std::wstring volumePath;
    if (sourceDrive.size() >= 2 && sourceDrive[1] == L':') {
        volumePath = L"\\\\.\\" + sourceDrive.substr(0, 2);
    } else {
        volumePath = sourceDrive;
    }

    NtfsParser parser;
    if (!parser.Open(volumePath)) return false;

    if (file.diskOffset == 0 || file.fileSize == 0) return false;

    // Calculate cluster number from disk offset
    uint64_t lcn = file.diskOffset / parser.GetBytesPerCluster();
    uint32_t clustersNeeded = static_cast<uint32_t>(
        (file.fileSize + parser.GetBytesPerCluster() - 1) / parser.GetBytesPerCluster());

    // Read clusters
    std::vector<uint8_t> data;
    if (!parser.ReadClusters(lcn, clustersNeeded, data))
        return false;

    // Trim to actual file size
    if (data.size() > file.fileSize)
        data.resize(static_cast<size_t>(file.fileSize));

    // Write to destination
    HANDLE hOut = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hOut, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);

    // Restore original timestamps
    if (file.creationTime.dwHighDateTime != 0 || file.modifiedTime.dwHighDateTime != 0) {
        SetFileTime(hOut,
                    file.creationTime.dwHighDateTime ? &file.creationTime : nullptr,
                    nullptr,
                    file.modifiedTime.dwHighDateTime ? &file.modifiedTime : nullptr);
    }

    CloseHandle(hOut);
    return ok && written == data.size();
}

bool Recovery::RecoverByCarving(const RecoveredFile& file,
                                const std::wstring& sourceDrive,
                                const std::wstring& destPath) {
    std::wstring volumePath;
    if (sourceDrive.size() >= 2 && sourceDrive[1] == L':') {
        volumePath = L"\\\\.\\" + sourceDrive.substr(0, 2);
    } else {
        volumePath = sourceDrive;
    }

    HANDLE hDisk = CreateFileW(volumePath.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
    if (hDisk == INVALID_HANDLE_VALUE) return false;

    // Determine actual file size
    uint64_t maxSize = file.fileSize;
    if (file.signatureId >= 0) {
        auto* sig = GetSignatureById(file.signatureId);
        if (sig) {
            maxSize = sig->maxFileSize;
            if (!sig->footer.empty()) {
                maxSize = FindActualSize(hDisk, file.diskOffset, sig->footer, sig->maxFileSize);
            }
        }
    }

    // Seek to file start (aligned to sector boundary)
    uint64_t alignedOffset = (file.diskOffset / 512) * 512;
    uint64_t skipBytes     = file.diskOffset - alignedOffset;

    LARGE_INTEGER pos;
    pos.QuadPart = static_cast<LONGLONG>(alignedOffset);
    if (!SetFilePointerEx(hDisk, pos, nullptr, FILE_BEGIN)) {
        CloseHandle(hDisk);
        return false;
    }

    // Read data
    uint64_t readSize = maxSize + skipBytes;
    uint64_t alignedReadSize = ((readSize + 511) / 512) * 512;

    // Cap at 100MB for carving to avoid excessive reads
    if (alignedReadSize > 100ULL * 1024 * 1024)
        alignedReadSize = 100ULL * 1024 * 1024;

    std::vector<uint8_t> buffer(static_cast<size_t>(alignedReadSize));
    DWORD bytesRead = 0;
    if (!ReadFile(hDisk, buffer.data(), static_cast<DWORD>(alignedReadSize), &bytesRead, nullptr)) {
        CloseHandle(hDisk);
        return false;
    }
    CloseHandle(hDisk);

    // Write to destination (skip alignment padding)
    size_t actualStart = static_cast<size_t>(skipBytes);
    size_t actualSize  = std::min(static_cast<size_t>(maxSize),
                                  static_cast<size_t>(bytesRead) - actualStart);

    HANDLE hOut = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hOut, buffer.data() + actualStart,
                        static_cast<DWORD>(actualSize), &written, nullptr);
    CloseHandle(hOut);

    return ok != FALSE;
}

uint64_t Recovery::FindActualSize(HANDLE hDisk, uint64_t startOffset,
                                  const std::vector<uint8_t>& footer,
                                  uint64_t maxSize) {
    if (footer.empty()) return maxSize;

    // Read in chunks looking for footer
    const uint32_t chunkSize = 1024 * 1024; // 1MB
    std::vector<uint8_t> buffer(chunkSize);

    uint64_t alignedStart = (startOffset / 512) * 512;
    LARGE_INTEGER pos;
    pos.QuadPart = static_cast<LONGLONG>(alignedStart);
    SetFilePointerEx(hDisk, pos, nullptr, FILE_BEGIN);

    uint64_t scanned = 0;
    while (scanned < maxSize) {
        DWORD bytesRead = 0;
        if (!ReadFile(hDisk, buffer.data(), chunkSize, &bytesRead, nullptr) || bytesRead == 0)
            break;

        // Search for footer
        for (uint32_t i = 0; i + footer.size() <= bytesRead; ++i) {
            if (std::equal(footer.begin(), footer.end(), buffer.data() + i)) {
                return scanned + i + footer.size() - (startOffset - alignedStart);
            }
        }

        scanned += bytesRead;
    }

    // Footer not found, use a reasonable default
    return std::min(maxSize, (uint64_t)10 * 1024 * 1024);
}

} // namespace rt
