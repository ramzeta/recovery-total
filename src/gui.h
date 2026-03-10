#pragma once
#include "utils.h"
#include "scanner.h"
#include "recovery.h"
#include "usb_diagnostic.h"
#include <windows.h>
#include <commctrl.h>
#include <vector>

namespace rt {

// ── Views ───────────────────────────────────────────────────────────────────
enum class AppView { Recovery, UsbDiagnostic };

// ── Window IDs ──────────────────────────────────────────────────────────────
// Navigation
constexpr int IDC_TAB_RECOVERY    = 1001;
constexpr int IDC_TAB_USB_DIAG    = 1002;

// Recovery view
constexpr int IDC_DRIVE_LIST      = 1010;
constexpr int IDC_FILE_LIST       = 1011;
constexpr int IDC_BTN_QUICK_SCAN  = 1012;
constexpr int IDC_BTN_DEEP_SCAN   = 1013;
constexpr int IDC_BTN_STOP        = 1014;
constexpr int IDC_BTN_RECOVER     = 1015;
constexpr int IDC_BTN_SELECT_ALL  = 1016;
constexpr int IDC_PROGRESS        = 1017;
constexpr int IDC_STATUS_LABEL    = 1018;
constexpr int IDC_FILTER_COMBO    = 1019;
constexpr int IDC_STATS_LABEL     = 1020;
constexpr int IDC_BTN_REFRESH     = 1021;
constexpr int IDC_STEP_LABEL      = 1022;

// USB Diagnostic view
constexpr int IDC_USB_LIST        = 1030;
constexpr int IDC_BTN_USB_REFRESH = 1031;
constexpr int IDC_BTN_DIAGNOSE    = 1032;
constexpr int IDC_USB_INFO_PANEL  = 1033;
constexpr int IDC_USB_HEALTH_BAR  = 1034;
constexpr int IDC_USB_STATUS      = 1035;
constexpr int IDC_USB_DETAIL_LIST = 1036;
constexpr int IDC_BTN_USB_RECOVER = 1037;

constexpr UINT WM_SCAN_PROGRESS   = WM_USER + 100;
constexpr UINT WM_SCAN_COMPLETE   = WM_USER + 101;

// ── Main application window ─────────────────────────────────────────────────
class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInstance);
    int  RunMessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // View management
    void SwitchView(AppView view);
    void ShowRecoveryView(bool show);
    void ShowUsbDiagView(bool show);

    // UI creation
    void CreateControls();
    void CreateNavBar();
    void CreateRecoveryControls();
    void CreateUsbDiagControls();
    void CreateFileListView();
    void ResizeControls();

    // Recovery view: Drive panel
    void PopulateDrives();
    void DrawDriveItem(DRAWITEMSTRUCT* dis);

    // Recovery view: Scan operations
    void StartScan(ScanType type);
    void StopScan();
    void OnScanProgress();
    void OnScanComplete();

    // Recovery view: File list
    void PopulateFileList();
    void ApplyFilter();
    void SelectAllFiles(bool select);

    // Recovery view: Recovery
    void RecoverSelected();
    std::wstring BrowseForFolder();

    // USB Diagnostic view
    void RefreshUsbList();
    void DrawUsbItem(DRAWITEMSTRUCT* dis);
    void RunDiagnostic();
    void DisplayDiagnosticResult(const UsbDeviceInfo& device, const DiagnosticResult& result);
    void ShowUsbDeviceInfo(int index);
    void RecoverFromUsb();

    // Drawing helpers
    void DrawRoundedRect(HDC hdc, RECT rc, int radius, COLORREF fill);
    void DrawNavButton(HDC hdc, RECT rc, const wchar_t* text, bool active);

    HINSTANCE hInstance_ = nullptr;
    HWND      hWnd_      = nullptr;
    AppView   currentView_ = AppView::UsbDiagnostic; // Start on USB diagnostic

    // ── Navigation ──
    HWND hTabRecovery_    = nullptr;
    HWND hTabUsbDiag_     = nullptr;

    // ── Recovery controls ──
    HWND hDriveList_      = nullptr;
    HWND hFileList_       = nullptr;
    HWND hBtnQuickScan_   = nullptr;
    HWND hBtnDeepScan_    = nullptr;
    HWND hBtnStop_        = nullptr;
    HWND hBtnRecover_     = nullptr;
    HWND hBtnSelectAll_   = nullptr;
    HWND hBtnRefresh_     = nullptr;
    HWND hProgress_       = nullptr;
    HWND hStatusLabel_    = nullptr;
    HWND hFilterCombo_    = nullptr;
    HWND hStatsLabel_     = nullptr;
    HWND hStepLabel_      = nullptr;

    // ── USB Diagnostic controls ──
    HWND hUsbList_        = nullptr;
    HWND hBtnUsbRefresh_  = nullptr;
    HWND hBtnDiagnose_    = nullptr;
    HWND hUsbStatus_      = nullptr;
    HWND hUsbDetailList_  = nullptr;
    HWND hUsbHealthBar_   = nullptr;
    HWND hBtnUsbRecover_  = nullptr;

    // ── Fonts & brushes ──
    HFONT hFontTitle_     = nullptr;
    HFONT hFontHeading_   = nullptr;
    HFONT hFontNormal_    = nullptr;
    HFONT hFontSmall_     = nullptr;
    HFONT hFontBold_      = nullptr;
    HFONT hFontHuge_      = nullptr;
    HBRUSH hBrushBg_      = nullptr;
    HBRUSH hBrushPanel_   = nullptr;
    HBRUSH hBrushAccent_  = nullptr;
    HBRUSH hBrushNavBg_   = nullptr;
    HBRUSH hBrushNavActive_ = nullptr;

    // ── Recovery state ──
    std::vector<DriveInfo>     drives_;
    std::vector<RecoveredFile> allResults_;
    std::vector<RecoveredFile> filteredResults_;
    int                        selectedDrive_ = -1;
    Scanner                    scanner_;
    std::wstring               currentFilter_ = L"All";
    bool                       allSelected_ = false;

    // ── USB Diagnostic state ──
    std::vector<UsbDeviceInfo>  usbDrives_;
    int                         selectedUsb_ = -1;
    DiagnosticResult            lastDiagResult_;
    bool                        diagRunning_ = false;
};

} // namespace rt
