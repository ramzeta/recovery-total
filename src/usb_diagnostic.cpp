#include "usb_diagnostic.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <ntddscsi.h>

#pragma comment(lib, "setupapi.lib")

namespace rt {

// ── Check if a physical drive is USB ────────────────────────────────────────
bool IsUsbBusType(int physicalDriveNum) {
    std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(physicalDriveNum);
    HANDLE h = CreateFileW(path.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType  = PropertyStandardQuery;

    uint8_t buffer[4096] = {};
    DWORD bytesReturned = 0;

    bool isUsb = false;
    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                        &query, sizeof(query),
                        buffer, sizeof(buffer),
                        &bytesReturned, nullptr)) {
        auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);
        isUsb = (desc->BusType == BusTypeUsb);
    }

    CloseHandle(h);
    return isUsb;
}

// ── Get physical drive number for a volume ──────────────────────────────────
static int GetPhysicalDriveNumber(const std::wstring& driveLetter) {
    // driveLetter like "E:\\"
    std::wstring volumePath = L"\\\\.\\" + driveLetter.substr(0, 2);
    HANDLE h = CreateFileW(volumePath.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;

    VOLUME_DISK_EXTENTS extents = {};
    DWORD bytesReturned = 0;
    int driveNum = -1;

    if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                        nullptr, 0,
                        &extents, sizeof(extents),
                        &bytesReturned, nullptr)) {
        if (extents.NumberOfDiskExtents > 0) {
            driveNum = static_cast<int>(extents.Extents[0].DiskNumber);
        }
    }

    CloseHandle(h);
    return driveNum;
}

// ── Get device descriptor info ──────────────────────────────────────────────
static void FillDeviceDescriptor(UsbDeviceInfo& info) {
    if (info.physicalDriveNum < 0) return;

    std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(info.physicalDriveNum);
    HANDLE h = CreateFileW(path.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType  = PropertyStandardQuery;

    uint8_t buffer[4096] = {};
    DWORD bytesReturned = 0;

    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                        &query, sizeof(query),
                        buffer, sizeof(buffer),
                        &bytesReturned, nullptr)) {
        auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);

        if (desc->VendorIdOffset > 0 && desc->VendorIdOffset < bytesReturned) {
            std::string vendor(reinterpret_cast<char*>(buffer + desc->VendorIdOffset));
            // Trim
            while (!vendor.empty() && (vendor.back() == ' ' || vendor.back() == '\0'))
                vendor.pop_back();
            info.manufacturer = std::wstring(vendor.begin(), vendor.end());
        }

        if (desc->ProductIdOffset > 0 && desc->ProductIdOffset < bytesReturned) {
            std::string product(reinterpret_cast<char*>(buffer + desc->ProductIdOffset));
            while (!product.empty() && (product.back() == ' ' || product.back() == '\0'))
                product.pop_back();
            info.productId = std::wstring(product.begin(), product.end());
        }

        if (desc->SerialNumberOffset > 0 && desc->SerialNumberOffset < bytesReturned) {
            std::string serial(reinterpret_cast<char*>(buffer + desc->SerialNumberOffset));
            while (!serial.empty() && (serial.back() == ' ' || serial.back() == '\0'))
                serial.pop_back();
            info.serialNumber = std::wstring(serial.begin(), serial.end());
        }

        // Determine media type description
        switch (desc->BusType) {
            case BusTypeUsb:   info.busType = L"USB"; break;
            case BusTypeSata:  info.busType = L"SATA"; break;
            case BusTypeNvme:  info.busType = L"NVMe"; break;
            case BusTypeScsi:  info.busType = L"SCSI"; break;
            default:           info.busType = L"Other"; break;
        }

        if (desc->RemovableMedia) {
            info.mediaType = L"Flash / Removable";
        } else {
            info.mediaType = L"Fixed Disk";
        }

        info.isRemovable = desc->RemovableMedia != 0;
    }

    // Check write protection
    DWORD writeProtect = 0;
    if (DeviceIoControl(h, IOCTL_DISK_IS_WRITABLE,
                        nullptr, 0, nullptr, 0,
                        &bytesReturned, nullptr)) {
        info.isWriteProtected = false;
    } else {
        if (GetLastError() == ERROR_WRITE_PROTECT) {
            info.isWriteProtected = true;
        }
    }

    CloseHandle(h);
}

// ── Enumerate USB drives ────────────────────────────────────────────────────
std::vector<UsbDeviceInfo> EnumerateUsbDrives() {
    std::vector<UsbDeviceInfo> results;

    // Enumerate logical drives and find removable/USB ones
    wchar_t buf[512];
    DWORD len = GetLogicalDriveStringsW(512, buf);

    for (DWORD i = 0; i < len;) {
        std::wstring root(&buf[i]);
        i += (DWORD)root.size() + 1;

        UINT driveType = GetDriveTypeW(root.c_str());
        if (driveType != DRIVE_REMOVABLE && driveType != DRIVE_FIXED)
            continue;

        int physNum = GetPhysicalDriveNumber(root);
        if (physNum < 0) continue;

        // Check if it's actually USB
        bool isUsb = IsUsbBusType(physNum);
        bool isRemovable = (driveType == DRIVE_REMOVABLE);

        if (!isUsb && !isRemovable) continue;

        UsbDeviceInfo info;
        info.driveLetter = root;
        info.physicalDriveNum = physNum;
        info.devicePath = L"\\\\.\\PhysicalDrive" + std::to_wstring(physNum);

        // Volume info
        wchar_t volName[MAX_PATH + 1] = {};
        wchar_t fsName[MAX_PATH + 1]  = {};
        if (GetVolumeInformationW(root.c_str(), volName, MAX_PATH,
                                  nullptr, nullptr, nullptr, fsName, MAX_PATH)) {
            info.volumeLabel = volName[0] ? volName : L"USB Drive";
            info.fileSystem  = fsName;
        } else {
            info.volumeLabel = L"USB Drive";
            info.fileSystem  = L"Unknown";
        }

        info.friendlyName = info.volumeLabel + L" (" + root.substr(0, 2) + L")";

        ULARGE_INTEGER totalBytes{}, freeBytes{};
        if (GetDiskFreeSpaceExW(root.c_str(), nullptr, &totalBytes, &freeBytes)) {
            info.totalBytes = totalBytes.QuadPart;
            info.freeBytes  = freeBytes.QuadPart;
        }

        // Get detailed device info
        FillDeviceDescriptor(info);

        results.push_back(std::move(info));
    }

    return results;
}

// ── Measure sequential read speed ───────────────────────────────────────────
double MeasureReadSpeed(const std::wstring& devicePath, uint64_t testSizeMB) {
    HANDLE h = CreateFileW(devicePath.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING,
                           FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0.0;

    const uint32_t blockSize = 1024 * 1024; // 1MB blocks
    std::vector<uint8_t> buffer(blockSize);
    uint64_t totalRead = 0;
    uint64_t targetBytes = testSizeMB * 1024 * 1024;

    auto start = std::chrono::high_resolution_clock::now();

    while (totalRead < targetBytes) {
        DWORD bytesRead = 0;
        if (!ReadFile(h, buffer.data(), blockSize, &bytesRead, nullptr) || bytesRead == 0)
            break;
        totalRead += bytesRead;
    }

    auto end = std::chrono::high_resolution_clock::now();
    CloseHandle(h);

    if (totalRead == 0) return 0.0;

    double seconds = std::chrono::duration<double>(end - start).count();
    if (seconds <= 0.001) return 0.0;

    return (totalRead / (1024.0 * 1024.0)) / seconds; // MB/s
}

// ── Run full diagnostic ─────────────────────────────────────────────────────
DiagnosticResult DiagnoseUsbDrive(const UsbDeviceInfo& device, bool) {
    DiagnosticResult result;
    int score = 100;

    // 1. Accessibility check
    HANDLE hVol = CreateFileW((L"\\\\.\\" + device.driveLetter.substr(0, 2)).c_str(),
                              GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    result.isAccessible = (hVol != INVALID_HANDLE_VALUE);
    if (hVol != INVALID_HANDLE_VALUE) CloseHandle(hVol);

    if (!result.isAccessible) {
        score -= 50;
        result.warnings.push_back(L"Drive is not accessible - may be corrupted or locked.");
        result.recommendations.push_back(L"Try ejecting and re-inserting the USB device.");
    }

    // 2. File system check
    result.fileSystemOk = !device.fileSystem.empty() &&
                          device.fileSystem != L"Unknown" &&
                          device.fileSystem != L"RAW";
    if (!result.fileSystemOk) {
        score -= 30;
        result.warnings.push_back(L"File system is not recognized (RAW or damaged).");
        result.recommendations.push_back(L"Format the drive or use Recovery to salvage data first.");
    }

    // 3. Free space check
    if (device.totalBytes > 0) {
        double usedPct = 1.0 - (double)device.freeBytes / device.totalBytes;
        if (usedPct > 0.95) {
            result.freeSpaceOk = false;
            score -= 10;
            result.warnings.push_back(L"Drive is almost full (>95% used).");
            result.recommendations.push_back(L"Free up space to prevent write errors.");
        }
    } else {
        score -= 20;
    }

    // 4. Read speed test
    result.seqReadSpeed = MeasureReadSpeed(device.devicePath, 16);
    result.readTestPassed = (result.seqReadSpeed > 1.0);

    if (result.seqReadSpeed <= 0.0) {
        score -= 25;
        result.warnings.push_back(L"Read speed test failed - drive may be failing.");
        result.recommendations.push_back(L"Backup all data immediately.");
    } else if (result.seqReadSpeed < 5.0) {
        score -= 15;
        result.warnings.push_back(L"Very slow read speed (" +
            std::to_wstring((int)result.seqReadSpeed) + L" MB/s) - possible hardware issues.");
        result.recommendations.push_back(L"Try a different USB port or cable.");
    } else if (result.seqReadSpeed < 20.0) {
        score -= 5;
        // USB 2.0 speeds are normal
    }

    // 5. Try to get SMART/health data via IOCTL
    if (device.physicalDriveNum >= 0) {
        std::wstring physPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(device.physicalDriveNum);
        HANDLE hPhys = CreateFileW(physPath.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hPhys != INVALID_HANDLE_VALUE) {
            // Check disk geometry for consistency
            DISK_GEOMETRY_EX geo = {};
            DWORD bytesReturned = 0;
            if (DeviceIoControl(hPhys, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                                nullptr, 0, &geo, sizeof(geo),
                                &bytesReturned, nullptr)) {
                // Compare reported size with actual geometry
                if (device.totalBytes > 0 && geo.DiskSize.QuadPart > 0) {
                    double sizeRatio = (double)device.totalBytes / geo.DiskSize.QuadPart;
                    if (sizeRatio < 0.5 || sizeRatio > 1.5) {
                        score -= 10;
                        result.warnings.push_back(L"Reported volume size doesn't match physical disk size.");
                    }
                }
            }

            CloseHandle(hPhys);
        }
    }

    // 6. Write protection check
    if (device.isWriteProtected) {
        score -= 5;
        result.warnings.push_back(L"Drive is write-protected.");
        result.recommendations.push_back(L"Check the physical write-protect switch on the device.");
    }

    // Calculate final score
    result.healthScore = std::max(0, std::min(100, score));

    if (result.healthScore >= 85)      result.overallHealth = HealthLevel::Excellent;
    else if (result.healthScore >= 60) result.overallHealth = HealthLevel::Good;
    else if (result.healthScore >= 30) result.overallHealth = HealthLevel::Warning;
    else                               result.overallHealth = HealthLevel::Critical;

    // Add general recommendations
    if (result.overallHealth == HealthLevel::Excellent) {
        result.recommendations.insert(result.recommendations.begin(),
            L"Drive is in good condition. No action needed.");
    }

    return result;
}

// ── Helpers ─────────────────────────────────────────────────────────────────
std::wstring HealthLevelToString(HealthLevel level) {
    switch (level) {
        case HealthLevel::Excellent: return L"Excellent";
        case HealthLevel::Good:      return L"Good";
        case HealthLevel::Warning:   return L"Warning";
        case HealthLevel::Critical:  return L"Critical";
        default:                     return L"Unknown";
    }
}

COLORREF HealthLevelToColor(HealthLevel level) {
    switch (level) {
        case HealthLevel::Excellent: return RGB(0, 200, 120);
        case HealthLevel::Good:      return RGB(0, 150, 255);
        case HealthLevel::Warning:   return RGB(255, 200, 60);
        case HealthLevel::Critical:  return RGB(255, 80, 80);
        default:                     return RGB(140, 140, 160);
    }
}

} // namespace rt
