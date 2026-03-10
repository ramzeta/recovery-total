#include "gui.h"
#include "utils.h"
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <string>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// MinGW uses WinMain; MSVC uses wWinMain
#ifdef __MINGW32__
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
#else
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
#endif
    // Initialize COM (needed for folder browser dialog)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    // Enable raw disk access privilege
    rt::EnablePrivilege(SE_BACKUP_NAME);
    rt::EnablePrivilege(SE_RESTORE_NAME);
    rt::EnablePrivilege(SE_MANAGE_VOLUME_NAME);

    // Create and run main window
    rt::MainWindow mainWnd;
    if (!mainWnd.Create(hInstance)) {
        std::wstring errMsg = L"Failed to create the main window.";
        std::wstring lastErr = rt::GetLastErrorMessage();
        if (!lastErr.empty())
            errMsg += L"\n\nError: " + lastErr;
        MessageBoxW(nullptr, errMsg.c_str(), L"Recovery Total", MB_ICONERROR);
        return 1;
    }

    int exitCode = mainWnd.RunMessageLoop();

    CoUninitialize();
    return exitCode;
}
