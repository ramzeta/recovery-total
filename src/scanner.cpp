#include "scanner.h"
#include <algorithm>

namespace rt {

Scanner::~Scanner() {
    StopScan();
}

bool Scanner::StartScan(const DriveInfo& drive, ScanType type,
                        ProgressCallback progressCb) {
    if (scanning_.load()) return false;

    stopRequested_ = false;
    scanning_ = true;

    {
        std::lock_guard<std::mutex> lk(resultsMutex_);
        results_.clear();
    }

    thread_ = std::thread(&Scanner::ScanThread, this, drive, type, progressCb);
    return true;
}

void Scanner::StopScan() {
    stopRequested_ = true;
    if (thread_.joinable()) thread_.join();
    scanning_ = false;
}

std::vector<RecoveredFile> Scanner::GetResults() {
    std::lock_guard<std::mutex> lk(resultsMutex_);
    return results_;
}

ScanProgress Scanner::GetProgress() {
    std::lock_guard<std::mutex> lk(progressMutex_);
    return currentProgress_;
}

void Scanner::ScanThread(DriveInfo drive, ScanType type, ProgressCallback progressCb) {
    auto updateProgress = [&](const ScanProgress& p) {
        {
            std::lock_guard<std::mutex> lk(progressMutex_);
            currentProgress_ = p;
        }
        if (progressCb) progressCb(p);
    };

    if (type == ScanType::QuickScan || type == ScanType::DeepScan) {
        RunMftScan(drive, updateProgress);
    }

    if ((type == ScanType::DeepScan || type == ScanType::CarvingOnly) && !stopRequested_) {
        RunCarvingScan(drive, updateProgress);
    }

    // Final progress
    ScanProgress final_p;
    {
        std::lock_guard<std::mutex> lk(resultsMutex_);
        final_p.filesFound = static_cast<uint32_t>(results_.size());
    }
    final_p.progressPct = 100.0;
    final_p.currentAction = L"Scan complete.";
    updateProgress(final_p);

    scanning_ = false;
}

void Scanner::RunMftScan(const DriveInfo& drive, ProgressCallback progressCb) {
    // For logical drives, convert "C:\\" to "\\\\.\\C:"
    std::wstring volumePath;
    if (!drive.isPhysical && drive.path.size() >= 2) {
        volumePath = L"\\\\.\\" + drive.path.substr(0, 2);
    } else {
        volumePath = drive.path;
    }

    NtfsParser parser;
    if (!parser.Open(volumePath)) {
        ScanProgress p;
        p.currentAction = L"Failed to open volume (not NTFS or access denied)";
        if (progressCb) progressCb(p);
        return;
    }

    std::vector<RecoveredFile> mftResults;
    parser.ScanDeletedFiles(mftResults, [&](const ScanProgress& p) {
        if (stopRequested_) return;
        if (progressCb) progressCb(p);
    });

    // Merge results
    {
        std::lock_guard<std::mutex> lk(resultsMutex_);
        results_.insert(results_.end(), mftResults.begin(), mftResults.end());
    }
}

void Scanner::RunCarvingScan(const DriveInfo& drive, ProgressCallback progressCb) {
    std::wstring path;
    if (!drive.isPhysical && drive.path.size() >= 2) {
        path = L"\\\\.\\" + drive.path.substr(0, 2);
    } else {
        path = drive.path;
    }

    HANDLE hDisk = CreateFileW(path.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING,
                               FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
                               nullptr);
    if (hDisk == INVALID_HANDLE_VALUE) return;

    // Read in 1MB chunks, scanning for file signatures
    const uint32_t chunkSize = 1024 * 1024;
    std::vector<uint8_t> buffer(chunkSize);
    uint64_t offset = 0;
    uint64_t totalSize = drive.totalBytes > 0 ? drive.totalBytes : 1;
    uint32_t carvedCount = 0;

    ScanProgress progress;
    progress.totalBytes = totalSize;
    progress.currentAction = L"Deep scan: searching for file signatures...";

    while (!stopRequested_) {
        DWORD bytesRead = 0;
        if (!ReadFile(hDisk, buffer.data(), chunkSize, &bytesRead, nullptr) || bytesRead == 0)
            break;

        // Scan every 512-byte sector boundary within this chunk
        for (uint32_t i = 0; i + 16 <= bytesRead; i += 512) {
            if (stopRequested_) break;

            auto matches = MatchSignatures(buffer.data() + i, bytesRead - i);
            for (int sigId : matches) {
                auto* sig = GetSignatureById(sigId);
                if (!sig) continue;

                RecoveredFile file;
                file.fileName    = L"Carved_" + std::to_wstring(++carvedCount) + sig->extension;
                file.extension   = sig->extension;
                file.diskOffset  = offset + i;
                file.fileSize    = sig->maxFileSize; // estimated max
                file.fromMFT     = false;
                file.signatureId = sigId;
                file.status      = FileStatus::Good;

                std::lock_guard<std::mutex> lk(resultsMutex_);
                results_.push_back(std::move(file));
            }
        }

        offset += bytesRead;

        // Update progress every few MB
        if ((offset % (16 * 1024 * 1024)) == 0) {
            progress.bytesScanned = offset;
            progress.progressPct  = 100.0 * offset / totalSize;
            {
                std::lock_guard<std::mutex> lk(resultsMutex_);
                progress.filesFound = static_cast<uint32_t>(results_.size());
            }
            if (progressCb) progressCb(progress);
        }
    }

    CloseHandle(hDisk);
}

} // namespace rt
