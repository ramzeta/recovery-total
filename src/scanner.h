#pragma once
#include "utils.h"
#include "ntfs_parser.h"
#include "file_signatures.h"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace rt {

enum class ScanType {
    QuickScan,    // MFT-only scan (fast, NTFS only)
    DeepScan,     // MFT + file carving (thorough)
    CarvingOnly   // Raw signature scan (for RAW/formatted drives)
};

class Scanner {
public:
    Scanner() = default;
    ~Scanner();

    // Start scanning a drive/volume
    bool StartScan(const DriveInfo& drive, ScanType type,
                   ProgressCallback progressCb = nullptr);

    // Stop a running scan
    void StopScan();

    // Check if scanning
    bool IsScanning() const { return scanning_.load(); }

    // Get results (thread-safe copy)
    std::vector<RecoveredFile> GetResults();

    // Get current progress
    ScanProgress GetProgress();

private:
    void ScanThread(DriveInfo drive, ScanType type, ProgressCallback progressCb);
    void RunMftScan(const DriveInfo& drive, ProgressCallback progressCb);
    void RunCarvingScan(const DriveInfo& drive, ProgressCallback progressCb);

    std::thread              thread_;
    std::atomic<bool>        scanning_{false};
    std::atomic<bool>        stopRequested_{false};
    std::mutex               resultsMutex_;
    std::vector<RecoveredFile> results_;
    ScanProgress             currentProgress_;
    std::mutex               progressMutex_;
};

} // namespace rt
