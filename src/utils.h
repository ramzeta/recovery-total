#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace rt {

// ── Drive info ──────────────────────────────────────────────────────────────
struct DriveInfo {
    std::wstring path;           // e.g. "\\\\.\\PhysicalDrive0" or "C:\\"
    std::wstring label;          // friendly name
    std::wstring fileSystem;     // NTFS, FAT32, exFAT, RAW...
    uint64_t     totalBytes = 0;
    uint64_t     freeBytes  = 0;
    DWORD        driveType  = 0; // DRIVE_FIXED, DRIVE_REMOVABLE...
    bool         isPhysical = false;
};

// ── Recovered file entry ────────────────────────────────────────────────────
enum class FileStatus { Excellent, Good, Poor, Unrecoverable };

struct RecoveredFile {
    std::wstring originalPath;
    std::wstring fileName;
    std::wstring extension;
    uint64_t     fileSize        = 0;
    uint64_t     diskOffset      = 0;  // byte offset on disk
    uint64_t     mftRecordIndex  = 0;
    FILETIME     creationTime    = {};
    FILETIME     modifiedTime    = {};
    FileStatus   status          = FileStatus::Good;
    bool         fromMFT         = false; // true = MFT, false = carving
    bool         selected        = false;
    int          signatureId     = -1;
};

// ── Scan progress callback ──────────────────────────────────────────────────
struct ScanProgress {
    uint64_t    bytesScanned   = 0;
    uint64_t    totalBytes     = 0;
    uint32_t    filesFound     = 0;
    std::wstring currentAction;
    double       progressPct   = 0.0;
};

using ProgressCallback = std::function<void(const ScanProgress&)>;

// ── Utility functions ───────────────────────────────────────────────────────
std::vector<DriveInfo> EnumerateDrives();
std::wstring           FormatFileSize(uint64_t bytes);
std::wstring           FormatFileTime(const FILETIME& ft);
std::wstring           FileStatusToString(FileStatus s);
bool                   EnablePrivilege(const wchar_t* privilege);
std::wstring           GetLastErrorMessage();
std::wstring           ExtensionFromName(const std::wstring& name);

} // namespace rt
