#pragma once
#include "utils.h"
#include <windows.h>
#include <winioctl.h>
#include <vector>
#include <string>
#include <cstdint>

namespace rt {

// ── USB device detailed info ────────────────────────────────────────────────
struct UsbDeviceInfo {
    std::wstring devicePath;
    std::wstring friendlyName;
    std::wstring manufacturer;
    std::wstring serialNumber;
    std::wstring productId;
    std::wstring busType;         // USB 2.0, USB 3.0, etc.
    std::wstring mediaType;       // HDD, SSD, Flash...
    uint64_t     totalBytes = 0;
    uint64_t     freeBytes  = 0;
    std::wstring fileSystem;
    std::wstring volumeLabel;
    std::wstring driveLetter;     // "E:\\"
    int          physicalDriveNum = -1;
    bool         isWriteProtected = false;
    bool         isRemovable = true;
};

// ── Diagnostic result ───────────────────────────────────────────────────────
enum class HealthLevel { Excellent, Good, Warning, Critical, Unknown };

struct DiagnosticResult {
    HealthLevel  overallHealth = HealthLevel::Unknown;
    int          healthScore   = -1;  // 0-100

    // Individual checks
    bool         isAccessible     = false;
    bool         fileSystemOk     = false;
    bool         freeSpaceOk      = true;
    bool         readTestPassed   = false;
    bool         smartAvailable   = false;

    // Speed test results (MB/s)
    double       seqReadSpeed     = 0.0;
    double       seqWriteSpeed    = 0.0;   // only if write test enabled

    // SMART-like data (from IOCTL)
    int          temperature      = -1;    // Celsius, -1 if unavailable
    uint64_t     powerOnHours     = 0;
    uint64_t     totalBytesRead   = 0;
    uint64_t     totalBytesWritten = 0;
    bool         hasBadSectors    = false;
    int          reallocatedSectors = 0;

    // Error info
    std::wstring errorDetails;
    std::vector<std::wstring> warnings;
    std::vector<std::wstring> recommendations;
};

// ── Functions ───────────────────────────────────────────────────────────────

// Enumerate only USB/removable drives with extended info
std::vector<UsbDeviceInfo> EnumerateUsbDrives();

// Run full diagnostic on a USB drive
DiagnosticResult DiagnoseUsbDrive(const UsbDeviceInfo& device, bool includeWriteTest = false);

// Quick read speed test (returns MB/s)
double MeasureReadSpeed(const std::wstring& devicePath, uint64_t testSizeMB = 32);

// Check if drive is USB based on bus type
bool IsUsbBusType(int physicalDriveNum);

// Get health level as string
std::wstring HealthLevelToString(HealthLevel level);

// Get health level color
COLORREF HealthLevelToColor(HealthLevel level);

} // namespace rt
