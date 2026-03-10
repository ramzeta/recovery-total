#include "utils.h"
#include <algorithm>
#include <winioctl.h>

namespace rt {

// ── Enumerate logical + physical drives ─────────────────────────────────────
std::vector<DriveInfo> EnumerateDrives() {
    std::vector<DriveInfo> drives;

    // Logical drives (C:\, D:\, etc.)
    wchar_t buf[512];
    DWORD len = GetLogicalDriveStringsW(512, buf);
    for (DWORD i = 0; i < len;) {
        std::wstring root(&buf[i]);
        i += (DWORD)root.size() + 1;

        DriveInfo di;
        di.path      = root;
        di.driveType = GetDriveTypeW(root.c_str());
        di.isPhysical = false;

        wchar_t volName[MAX_PATH + 1] = {};
        wchar_t fsName[MAX_PATH + 1]  = {};
        if (GetVolumeInformationW(root.c_str(), volName, MAX_PATH,
                                  nullptr, nullptr, nullptr, fsName, MAX_PATH)) {
            di.label      = volName[0] ? volName : L"Local Disk";
            di.fileSystem = fsName;
        } else {
            di.label     = L"Unknown";
            di.fileSystem = L"RAW";
        }

        ULARGE_INTEGER totalBytes{}, freeBytes{};
        if (GetDiskFreeSpaceExW(root.c_str(), nullptr, &totalBytes, &freeBytes)) {
            di.totalBytes = totalBytes.QuadPart;
            di.freeBytes  = freeBytes.QuadPart;
        }

        drives.push_back(di);
    }

    // Physical drives (\\.\PhysicalDrive0, 1, 2...)
    for (int i = 0; i < 16; ++i) {
        std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(i);
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        DriveInfo di;
        di.path       = path;
        di.isPhysical = true;
        di.label      = L"Physical Drive " + std::to_wstring(i);
        di.fileSystem = L"RAW";

        DISK_GEOMETRY_EX geo = {};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                            nullptr, 0, &geo, sizeof(geo), &bytesReturned, nullptr)) {
            di.totalBytes = geo.DiskSize.QuadPart;
        }

        CloseHandle(h);
        drives.push_back(di);
    }

    return drives;
}

// ── Format helpers ──────────────────────────────────────────────────────────
std::wstring FormatFileSize(uint64_t bytes) {
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int idx = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && idx < 4) { size /= 1024.0; ++idx; }

    std::wostringstream oss;
    oss << std::fixed << std::setprecision(idx == 0 ? 0 : 2) << size << L" " << units[idx];
    return oss.str();
}

std::wstring FormatFileTime(const FILETIME& ft) {
    if (ft.dwHighDateTime == 0 && ft.dwLowDateTime == 0)
        return L"Unknown";

    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    std::wostringstream oss;
    oss << std::setfill(L'0')
        << st.wYear << L"-"
        << std::setw(2) << st.wMonth << L"-"
        << std::setw(2) << st.wDay << L" "
        << std::setw(2) << st.wHour << L":"
        << std::setw(2) << st.wMinute;
    return oss.str();
}

std::wstring FileStatusToString(FileStatus s) {
    switch (s) {
        case FileStatus::Excellent:      return L"Excellent";
        case FileStatus::Good:           return L"Good";
        case FileStatus::Poor:           return L"Poor";
        case FileStatus::Unrecoverable:  return L"Unrecoverable";
    }
    return L"Unknown";
}

bool EnablePrivilege(const wchar_t* privilege) {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;

    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(nullptr, privilege, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr);
    CloseHandle(hToken);
    return ok && GetLastError() != ERROR_NOT_ALL_ASSIGNED;
}

std::wstring GetLastErrorMessage() {
    DWORD err = GetLastError();
    if (err == 0) return L"";
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   nullptr, err, 0, (LPWSTR)&msg, 0, nullptr);
    std::wstring result = msg ? msg : L"Unknown error";
    if (msg) LocalFree(msg);
    return result;
}

std::wstring ExtensionFromName(const std::wstring& name) {
    auto pos = name.rfind(L'.');
    if (pos == std::wstring::npos) return L"";
    std::wstring ext = name.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext;
}

} // namespace rt
