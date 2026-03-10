#include "gui.h"
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace rt {

// ── Colors ──────────────────────────────────────────────────────────────────
static constexpr COLORREF CLR_BG         = RGB(18, 18, 26);
static constexpr COLORREF CLR_NAV_BG     = RGB(12, 12, 18);
static constexpr COLORREF CLR_NAV_ACTIVE = RGB(0, 120, 215);
static constexpr COLORREF CLR_PANEL      = RGB(30, 30, 42);
static constexpr COLORREF CLR_PANEL_ALT  = RGB(38, 38, 52);
static constexpr COLORREF CLR_CARD       = RGB(40, 40, 56);
static constexpr COLORREF CLR_ACCENT     = RGB(0, 150, 255);
static constexpr COLORREF CLR_ACCENT2    = RGB(0, 200, 120);
static constexpr COLORREF CLR_RED        = RGB(255, 80, 80);
static constexpr COLORREF CLR_YELLOW     = RGB(255, 200, 60);
static constexpr COLORREF CLR_ORANGE     = RGB(255, 150, 50);
static constexpr COLORREF CLR_TEXT       = RGB(230, 230, 240);
static constexpr COLORREF CLR_TEXT_DIM   = RGB(140, 140, 160);
static constexpr COLORREF CLR_TEXT_HINT  = RGB(100, 100, 120);
static constexpr COLORREF CLR_SELECTED   = RGB(0, 100, 200);
static constexpr COLORREF CLR_BORDER     = RGB(55, 55, 75);

// Nav bar height and layout constants
static constexpr int NAV_HEIGHT     = 56;
static constexpr int LEFT_PANEL_W   = 300;
static constexpr int TOOLBAR_H      = 50;
static constexpr int BOTTOM_BAR_H   = 70;

MainWindow::MainWindow() {
    hBrushBg_        = CreateSolidBrush(CLR_BG);
    hBrushPanel_     = CreateSolidBrush(CLR_PANEL);
    hBrushAccent_    = CreateSolidBrush(CLR_ACCENT);
    hBrushNavBg_     = CreateSolidBrush(CLR_NAV_BG);
    hBrushNavActive_ = CreateSolidBrush(CLR_NAV_ACTIVE);
}

MainWindow::~MainWindow() {
    if (hFontTitle_)     DeleteObject(hFontTitle_);
    if (hFontHeading_)   DeleteObject(hFontHeading_);
    if (hFontNormal_)    DeleteObject(hFontNormal_);
    if (hFontSmall_)     DeleteObject(hFontSmall_);
    if (hFontBold_)      DeleteObject(hFontBold_);
    if (hFontHuge_)      DeleteObject(hFontHuge_);
    if (hBrushBg_)       DeleteObject(hBrushBg_);
    if (hBrushPanel_)    DeleteObject(hBrushPanel_);
    if (hBrushAccent_)   DeleteObject(hBrushAccent_);
    if (hBrushNavBg_)    DeleteObject(hBrushNavBg_);
    if (hBrushNavActive_) DeleteObject(hBrushNavActive_);
}

bool MainWindow::Create(HINSTANCE hInstance) {
    hInstance_ = hInstance;

    hFontTitle_   = CreateFontW(-22, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hFontHeading_ = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hFontNormal_  = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hFontSmall_   = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hFontBold_    = CreateFontW(-14, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hFontHuge_    = CreateFontW(-48, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = hBrushBg_;
    wc.lpszClassName = L"RecoveryTotalWnd";
    wc.hIcon         = LoadIcon(nullptr, IDI_SHIELD);
    wc.hIconSm       = LoadIcon(nullptr, IDI_SHIELD);
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1350, winH = 850;
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;

    hWnd_ = CreateWindowExW(
        0, L"RecoveryTotalWnd",
        L"Recovery Total - USB Diagnostic & Data Recovery",
        WS_OVERLAPPEDWINDOW,
        x, y, winW, winH,
        nullptr, nullptr, hInstance, this);

    if (!hWnd_) return false;

    CreateControls();

    // Start on USB Diagnostic view
    SwitchView(AppView::UsbDiagnostic);

    ShowWindow(hWnd_, SW_SHOW);
    UpdateWindow(hWnd_);
    return true;
}

int MainWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hWnd_ = hWnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        ResizeControls();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        // Navigation
        case IDC_TAB_USB_DIAG:   SwitchView(AppView::UsbDiagnostic); return 0;
        case IDC_TAB_RECOVERY:   SwitchView(AppView::Recovery); return 0;

        // Recovery buttons
        case IDC_BTN_QUICK_SCAN: StartScan(ScanType::QuickScan); return 0;
        case IDC_BTN_DEEP_SCAN:  StartScan(ScanType::DeepScan);  return 0;
        case IDC_BTN_STOP:       StopScan(); return 0;
        case IDC_BTN_RECOVER:    RecoverSelected(); return 0;
        case IDC_BTN_SELECT_ALL: SelectAllFiles(!allSelected_); return 0;
        case IDC_BTN_REFRESH:    PopulateDrives(); return 0;

        // USB Diagnostic buttons
        case IDC_BTN_USB_REFRESH: RefreshUsbList(); return 0;
        case IDC_BTN_DIAGNOSE:    RunDiagnostic(); return 0;
        case IDC_BTN_USB_RECOVER: RecoverFromUsb(); return 0;

        case IDC_FILTER_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                wchar_t buf[64];
                int sel = (int)SendMessage(hFilterCombo_, CB_GETCURSEL, 0, 0);
                SendMessage(hFilterCombo_, CB_GETLBTEXT, sel, (LPARAM)buf);
                currentFilter_ = buf;
                ApplyFilter();
            }
            return 0;

        case IDC_USB_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                selectedUsb_ = (int)SendMessage(hUsbList_, LB_GETCURSEL, 0, 0);
                ShowUsbDeviceInfo(selectedUsb_);
            }
            return 0;
        }
        break;

    case WM_NOTIFY: {
        auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->idFrom == IDC_DRIVE_LIST && nmhdr->code == LBN_SELCHANGE) {
            selectedDrive_ = (int)SendMessage(hDriveList_, LB_GETCURSEL, 0, 0);
        }
        if (nmhdr->idFrom == IDC_FILE_LIST && nmhdr->code == NM_CLICK) {
            auto* nmItem = reinterpret_cast<NMITEMACTIVATE*>(lParam);
            if (nmItem->iItem >= 0 && nmItem->iItem < (int)filteredResults_.size()) {
                filteredResults_[nmItem->iItem].selected = !filteredResults_[nmItem->iItem].selected;
                ListView_RedrawItems(hFileList_, nmItem->iItem, nmItem->iItem);
            }
        }
        if (nmhdr->idFrom == IDC_FILE_LIST && nmhdr->code == LVN_GETDISPINFOW) {
            auto* lvdi = reinterpret_cast<NMLVDISPINFOW*>(lParam);
            int idx = lvdi->item.iItem;
            if (idx < 0 || idx >= (int)filteredResults_.size()) break;

            auto& f = filteredResults_[idx];
            static wchar_t textBuf[512];

            if (lvdi->item.mask & LVIF_TEXT) {
                switch (lvdi->item.iSubItem) {
                case 0: wcscpy_s(textBuf, f.selected ? L"\u2611" : L"\u2610"); break;
                case 1: wcsncpy_s(textBuf, f.fileName.c_str(), 255); break;
                case 2: wcsncpy_s(textBuf, FormatFileSize(f.fileSize).c_str(), 63); break;
                case 3: wcsncpy_s(textBuf, f.extension.c_str(), 31); break;
                case 4: wcsncpy_s(textBuf, FormatFileTime(f.modifiedTime).c_str(), 63); break;
                case 5: wcsncpy_s(textBuf, FileStatusToString(f.status).c_str(), 31); break;
                case 6: wcscpy_s(textBuf, f.fromMFT ? L"MFT" : L"Carving"); break;
                }
                lvdi->item.pszText = textBuf;
            }
        }
        // Custom draw for file list
        if (nmhdr->idFrom == IDC_FILE_LIST && nmhdr->code == NM_CUSTOMDRAW) {
            auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                int idx = (int)cd->nmcd.dwItemSpec;
                if (idx >= 0 && idx < (int)filteredResults_.size()) {
                    auto& f = filteredResults_[idx];
                    cd->clrText = CLR_TEXT;
                    cd->clrTextBk = (idx % 2 == 0) ? CLR_PANEL : CLR_PANEL_ALT;
                    if (f.selected) cd->clrTextBk = CLR_SELECTED;
                }
                return CDRF_NOTIFYSUBITEMDRAW;
            }
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                int idx = (int)cd->nmcd.dwItemSpec;
                if (idx >= 0 && idx < (int)filteredResults_.size()) {
                    auto& f = filteredResults_[idx];
                    cd->clrText = CLR_TEXT;
                    cd->clrTextBk = (idx % 2 == 0) ? CLR_PANEL : CLR_PANEL_ALT;
                    if (f.selected) cd->clrTextBk = CLR_SELECTED;
                    if (cd->iSubItem == 5) {
                        switch (f.status) {
                        case FileStatus::Excellent:     cd->clrText = CLR_ACCENT2; break;
                        case FileStatus::Good:          cd->clrText = CLR_ACCENT; break;
                        case FileStatus::Poor:          cd->clrText = CLR_YELLOW; break;
                        case FileStatus::Unrecoverable: cd->clrText = CLR_RED; break;
                        }
                    }
                }
                return CDRF_NEWFONT;
            }
            }
        }
        break;
    }

    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlID == IDC_DRIVE_LIST) {
            DrawDriveItem(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_USB_LIST) {
            DrawUsbItem(dis);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_PANEL);
        return (LRESULT)hBrushPanel_;
    }

    case WM_SCAN_PROGRESS:
        OnScanProgress();
        return 0;

    case WM_SCAN_COMPLETE:
        OnScanComplete();
        return 0;

    case WM_DESTROY:
        scanner_.StopScan();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd_, msg, wParam, lParam);
}

// ═══════════════════════════════════════════════════════════════════════════
//  VIEW MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

void MainWindow::SwitchView(AppView view) {
    currentView_ = view;

    ShowRecoveryView(view == AppView::Recovery);
    ShowUsbDiagView(view == AppView::UsbDiagnostic);

    // Update nav button appearance
    SetWindowTextW(hTabRecovery_,
        view == AppView::Recovery ? L">> Recuperar Datos" : L"   Recuperar Datos");
    SetWindowTextW(hTabUsbDiag_,
        view == AppView::UsbDiagnostic ? L">> Diagnosticar USB" : L"   Diagnosticar USB");

    if (view == AppView::Recovery) {
        PopulateDrives();
    } else {
        RefreshUsbList();
    }

    InvalidateRect(hWnd_, nullptr, TRUE);
}

void MainWindow::ShowRecoveryView(bool show) {
    int cmd = show ? SW_SHOW : SW_HIDE;
    if (hDriveList_)     ShowWindow(hDriveList_, cmd);
    if (hFileList_)      ShowWindow(hFileList_, cmd);
    if (hBtnQuickScan_)  ShowWindow(hBtnQuickScan_, cmd);
    if (hBtnDeepScan_)   ShowWindow(hBtnDeepScan_, cmd);
    if (hBtnStop_)       ShowWindow(hBtnStop_, cmd);
    if (hBtnRecover_)    ShowWindow(hBtnRecover_, cmd);
    if (hBtnSelectAll_)  ShowWindow(hBtnSelectAll_, cmd);
    if (hBtnRefresh_)    ShowWindow(hBtnRefresh_, cmd);
    if (hProgress_)      ShowWindow(hProgress_, cmd);
    if (hStatusLabel_)   ShowWindow(hStatusLabel_, cmd);
    if (hFilterCombo_)   ShowWindow(hFilterCombo_, cmd);
    if (hStatsLabel_)    ShowWindow(hStatsLabel_, cmd);
    if (hStepLabel_)     ShowWindow(hStepLabel_, cmd);
}

void MainWindow::ShowUsbDiagView(bool show) {
    int cmd = show ? SW_SHOW : SW_HIDE;
    if (hUsbList_)       ShowWindow(hUsbList_, cmd);
    if (hBtnUsbRefresh_) ShowWindow(hBtnUsbRefresh_, cmd);
    if (hBtnDiagnose_)   ShowWindow(hBtnDiagnose_, cmd);
    if (hUsbStatus_)     ShowWindow(hUsbStatus_, cmd);
    if (hUsbDetailList_) ShowWindow(hUsbDetailList_, cmd);
    if (hUsbHealthBar_)  ShowWindow(hUsbHealthBar_, cmd);
    if (hBtnUsbRecover_) ShowWindow(hBtnUsbRecover_, cmd);
}

// ═══════════════════════════════════════════════════════════════════════════
//  CONTROL CREATION
// ═══════════════════════════════════════════════════════════════════════════

void MainWindow::CreateControls() {
    CreateNavBar();
    CreateRecoveryControls();
    CreateUsbDiagControls();
}

void MainWindow::CreateNavBar() {
    RECT rc;
    GetClientRect(hWnd_, &rc);
    int w = rc.right;

    // Nav buttons (owner-drawn would be better, but using styled buttons for simplicity)
    hTabUsbDiag_ = CreateWindowW(L"BUTTON", L">> Diagnosticar USB",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_LEFT,
        0, 0, w / 2, NAV_HEIGHT, hWnd_,
        (HMENU)(UINT_PTR)IDC_TAB_USB_DIAG, hInstance_, nullptr);
    SendMessage(hTabUsbDiag_, WM_SETFONT, (WPARAM)hFontHeading_, TRUE);

    hTabRecovery_ = CreateWindowW(L"BUTTON", L"   Recuperar Datos",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_LEFT,
        w / 2, 0, w / 2, NAV_HEIGHT, hWnd_,
        (HMENU)(UINT_PTR)IDC_TAB_RECOVERY, hInstance_, nullptr);
    SendMessage(hTabRecovery_, WM_SETFONT, (WPARAM)hFontHeading_, TRUE);
}

void MainWindow::CreateRecoveryControls() {
    RECT rc;
    GetClientRect(hWnd_, &rc);
    int w = rc.right, h = rc.bottom;
    int topY = NAV_HEIGHT + 5;

    // ── Step indicator ──
    hStepLabel_ = CreateWindowW(L"STATIC",
        L"  PASO 1: Selecciona unidad   |   PASO 2: Escanear   |   PASO 3: Recuperar",
        WS_CHILD | SS_LEFT,
        0, topY, w, 28, hWnd_,
        (HMENU)(UINT_PTR)IDC_STEP_LABEL, hInstance_, nullptr);
    SendMessage(hStepLabel_, WM_SETFONT, (WPARAM)hFontSmall_, TRUE);

    topY += 32;

    // ── Drive list (left panel) ──
    hDriveList_ = CreateWindowExW(0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
        10, topY, LEFT_PANEL_W - 20, h - topY - BOTTOM_BAR_H - 45,
        hWnd_, (HMENU)(UINT_PTR)IDC_DRIVE_LIST, hInstance_, nullptr);
    SendMessage(hDriveList_, LB_SETITEMHEIGHT, 0, 70);
    SendMessage(hDriveList_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    // ── Refresh drives button ──
    hBtnRefresh_ = CreateWindowW(L"BUTTON", L"Actualizar unidades",
        WS_CHILD | BS_PUSHBUTTON,
        10, h - BOTTOM_BAR_H - 10, LEFT_PANEL_W - 20, 32, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_REFRESH, hInstance_, nullptr);
    SendMessage(hBtnRefresh_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    // ── Toolbar (top right) ──
    int toolX = LEFT_PANEL_W + 5;
    int toolY = topY;

    hBtnQuickScan_ = CreateWindowW(L"BUTTON", L"Escaneo rapido (MFT)",
        WS_CHILD | BS_PUSHBUTTON,
        toolX, toolY, 170, 36, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_QUICK_SCAN, hInstance_, nullptr);
    SendMessage(hBtnQuickScan_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    hBtnDeepScan_ = CreateWindowW(L"BUTTON", L"Escaneo profundo",
        WS_CHILD | BS_PUSHBUTTON,
        toolX + 180, toolY, 160, 36, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_DEEP_SCAN, hInstance_, nullptr);
    SendMessage(hBtnDeepScan_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    hBtnStop_ = CreateWindowW(L"BUTTON", L"Detener",
        WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
        toolX + 350, toolY, 90, 36, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_STOP, hInstance_, nullptr);
    SendMessage(hBtnStop_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hBtnSelectAll_ = CreateWindowW(L"BUTTON", L"Seleccionar todo",
        WS_CHILD | BS_PUSHBUTTON,
        toolX + 450, toolY, 140, 36, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_SELECT_ALL, hInstance_, nullptr);
    SendMessage(hBtnSelectAll_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    hBtnRecover_ = CreateWindowW(L"BUTTON", L"RECUPERAR SELECCIONADOS",
        WS_CHILD | BS_PUSHBUTTON,
        toolX + 600, toolY, 220, 36, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_RECOVER, hInstance_, nullptr);
    SendMessage(hBtnRecover_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    // ── Filter combo ──
    hFilterCombo_ = CreateWindowW(L"COMBOBOX", nullptr,
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        toolX + 830, toolY, 140, 200, hWnd_,
        (HMENU)(UINT_PTR)IDC_FILTER_COMBO, hInstance_, nullptr);
    SendMessage(hFilterCombo_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);
    const wchar_t* filters[] = {
        L"Todos", L"Imagenes", L"Documentos", L"Videos", L"Audio", L"Archivos", L"Programas"
    };
    for (auto& f : filters)
        SendMessage(hFilterCombo_, CB_ADDSTRING, 0, (LPARAM)f);
    SendMessage(hFilterCombo_, CB_SETCURSEL, 0, 0);

    // ── File list view ──
    CreateFileListView();

    // ── Progress bar ──
    hProgress_ = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | PBS_SMOOTH,
        LEFT_PANEL_W + 5, h - BOTTOM_BAR_H + 5, w - LEFT_PANEL_W - 230, 22,
        hWnd_, (HMENU)(UINT_PTR)IDC_PROGRESS, hInstance_, nullptr);
    SendMessage(hProgress_, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
    SendMessage(hProgress_, PBM_SETBARCOLOR, 0, CLR_ACCENT);
    SendMessage(hProgress_, PBM_SETBKCOLOR, 0, CLR_PANEL);

    // ── Status label ──
    hStatusLabel_ = CreateWindowW(L"STATIC",
        L"Selecciona una unidad y pulsa Escanear para comenzar.",
        WS_CHILD | SS_LEFT,
        LEFT_PANEL_W + 5, h - BOTTOM_BAR_H + 32, w - LEFT_PANEL_W - 20, 22,
        hWnd_, (HMENU)(UINT_PTR)IDC_STATUS_LABEL, hInstance_, nullptr);
    SendMessage(hStatusLabel_, WM_SETFONT, (WPARAM)hFontSmall_, TRUE);

    // ── Stats label ──
    hStatsLabel_ = CreateWindowW(L"STATIC", L"",
        WS_CHILD | SS_RIGHT,
        w - 220, h - BOTTOM_BAR_H + 5, 210, 22, hWnd_,
        (HMENU)(UINT_PTR)IDC_STATS_LABEL, hInstance_, nullptr);
    SendMessage(hStatsLabel_, WM_SETFONT, (WPARAM)hFontSmall_, TRUE);
}

void MainWindow::CreateFileListView() {
    RECT rc;
    GetClientRect(hWnd_, &rc);
    int w = rc.right, h = rc.bottom;
    int topY = NAV_HEIGHT + 5 + 32 + TOOLBAR_H;

    hFileList_ = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_BORDER |
        LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS,
        LEFT_PANEL_W + 5, topY, w - LEFT_PANEL_W - 15, h - topY - BOTTOM_BAR_H - 5,
        hWnd_, (HMENU)(UINT_PTR)IDC_FILE_LIST, hInstance_, nullptr);

    ListView_SetExtendedListViewStyle(hFileList_,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

    SendMessage(hFileList_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    ListView_SetBkColor(hFileList_, CLR_PANEL);
    ListView_SetTextBkColor(hFileList_, CLR_PANEL);
    ListView_SetTextColor(hFileList_, CLR_TEXT);

    struct { const wchar_t* name; int width; int fmt; } cols[] = {
        { L"",           30,  LVCFMT_CENTER },
        { L"Archivo",    280, LVCFMT_LEFT },
        { L"Tamano",     90,  LVCFMT_RIGHT },
        { L"Tipo",       70,  LVCFMT_CENTER },
        { L"Fecha",      130, LVCFMT_CENTER },
        { L"Estado",     90,  LVCFMT_CENTER },
        { L"Fuente",     70,  LVCFMT_CENTER },
    };

    for (int i = 0; i < 7; ++i) {
        LVCOLUMNW lvc = {};
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
        lvc.fmt  = cols[i].fmt;
        lvc.cx   = cols[i].width;
        lvc.pszText = const_cast<wchar_t*>(cols[i].name);
        ListView_InsertColumn(hFileList_, i, &lvc);
    }
}

void MainWindow::CreateUsbDiagControls() {
    RECT rc;
    GetClientRect(hWnd_, &rc);
    int w = rc.right, h = rc.bottom;
    int topY = NAV_HEIGHT + 10;
    int leftW = 320;

    // ── USB Device list (left panel) ──
    hUsbList_ = CreateWindowExW(0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
        10, topY, leftW - 20, h - topY - 60,
        hWnd_, (HMENU)(UINT_PTR)IDC_USB_LIST, hInstance_, nullptr);
    SendMessage(hUsbList_, LB_SETITEMHEIGHT, 0, 80);
    SendMessage(hUsbList_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    // ── USB Refresh button ──
    hBtnUsbRefresh_ = CreateWindowW(L"BUTTON", L"Actualizar dispositivos USB",
        WS_CHILD | BS_PUSHBUTTON,
        10, h - 50, leftW - 20, 35, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_USB_REFRESH, hInstance_, nullptr);
    SendMessage(hBtnUsbRefresh_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);

    // ── Right panel: Diagnostic area ──
    int rightX = leftW + 10;
    int rightW = w - rightX - 10;

    // Health progress bar (acts as health meter)
    hUsbHealthBar_ = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | PBS_SMOOTH,
        rightX, topY + 70, rightW, 30,
        hWnd_, (HMENU)(UINT_PTR)IDC_USB_HEALTH_BAR, hInstance_, nullptr);
    SendMessage(hUsbHealthBar_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(hUsbHealthBar_, PBM_SETBARCOLOR, 0, CLR_ACCENT2);
    SendMessage(hUsbHealthBar_, PBM_SETBKCOLOR, 0, CLR_PANEL);

    // Status label for diagnostic info
    hUsbStatus_ = CreateWindowW(L"STATIC",
        L"  Conecta un dispositivo USB y seleccionalo de la lista.\n"
        L"  Pulsa \"Diagnosticar\" para analizar el estado del dispositivo.",
        WS_CHILD | SS_LEFT,
        rightX, topY, rightW, 65,
        hWnd_, (HMENU)(UINT_PTR)IDC_USB_STATUS, hInstance_, nullptr);
    SendMessage(hUsbStatus_, WM_SETFONT, (WPARAM)hFontHeading_, TRUE);

    // Detail list (shows diagnostic results)
    hUsbDetailList_ = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
        rightX, topY + 110, rightW, h - topY - 180,
        hWnd_, (HMENU)(UINT_PTR)IDC_USB_DETAIL_LIST, hInstance_, nullptr);

    ListView_SetExtendedListViewStyle(hUsbDetailList_,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    SendMessage(hUsbDetailList_, WM_SETFONT, (WPARAM)hFontNormal_, TRUE);
    ListView_SetBkColor(hUsbDetailList_, CLR_PANEL);
    ListView_SetTextBkColor(hUsbDetailList_, CLR_PANEL);
    ListView_SetTextColor(hUsbDetailList_, CLR_TEXT);

    // Detail columns
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

    lvc.fmt = LVCFMT_LEFT; lvc.cx = 250;
    lvc.pszText = const_cast<wchar_t*>(L"Propiedad");
    ListView_InsertColumn(hUsbDetailList_, 0, &lvc);

    lvc.fmt = LVCFMT_LEFT; lvc.cx = rightW - 270;
    lvc.pszText = const_cast<wchar_t*>(L"Valor");
    ListView_InsertColumn(hUsbDetailList_, 1, &lvc);

    // ── Bottom buttons ──
    hBtnDiagnose_ = CreateWindowW(L"BUTTON", L"DIAGNOSTICAR",
        WS_CHILD | BS_PUSHBUTTON,
        rightX, h - 55, 200, 40, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_DIAGNOSE, hInstance_, nullptr);
    SendMessage(hBtnDiagnose_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    hBtnUsbRecover_ = CreateWindowW(L"BUTTON", L"Recuperar datos de este USB",
        WS_CHILD | BS_PUSHBUTTON,
        rightX + 220, h - 55, 260, 40, hWnd_,
        (HMENU)(UINT_PTR)IDC_BTN_USB_RECOVER, hInstance_, nullptr);
    SendMessage(hBtnUsbRecover_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);
}

void MainWindow::ResizeControls() {
    if (!hWnd_) return;
    RECT rc;
    GetClientRect(hWnd_, &rc);
    int w = rc.right, h = rc.bottom;
    int topY = NAV_HEIGHT + 5;

    // Nav
    if (hTabUsbDiag_)  MoveWindow(hTabUsbDiag_, 0, 0, w / 2, NAV_HEIGHT, TRUE);
    if (hTabRecovery_) MoveWindow(hTabRecovery_, w / 2, 0, w / 2, NAV_HEIGHT, TRUE);

    // Recovery view
    topY += 32; // step label
    if (hStepLabel_)    MoveWindow(hStepLabel_, 0, NAV_HEIGHT + 5, w, 28, TRUE);
    if (hDriveList_)    MoveWindow(hDriveList_, 10, topY, LEFT_PANEL_W - 20, h - topY - BOTTOM_BAR_H - 45, TRUE);
    if (hBtnRefresh_)   MoveWindow(hBtnRefresh_, 10, h - BOTTOM_BAR_H - 10, LEFT_PANEL_W - 20, 32, TRUE);
    if (hFileList_)     MoveWindow(hFileList_, LEFT_PANEL_W + 5, topY + TOOLBAR_H, w - LEFT_PANEL_W - 15, h - topY - TOOLBAR_H - BOTTOM_BAR_H - 5, TRUE);
    if (hProgress_)     MoveWindow(hProgress_, LEFT_PANEL_W + 5, h - BOTTOM_BAR_H + 5, w - LEFT_PANEL_W - 230, 22, TRUE);
    if (hStatusLabel_)  MoveWindow(hStatusLabel_, LEFT_PANEL_W + 5, h - BOTTOM_BAR_H + 32, w - LEFT_PANEL_W - 20, 22, TRUE);
    if (hStatsLabel_)   MoveWindow(hStatsLabel_, w - 220, h - BOTTOM_BAR_H + 5, 210, 22, TRUE);

    // USB Diagnostic view
    int usbTopY = NAV_HEIGHT + 10;
    int leftW = 320;
    int rightX = leftW + 10;
    int rightW = w - rightX - 10;

    if (hUsbList_)       MoveWindow(hUsbList_, 10, usbTopY, leftW - 20, h - usbTopY - 60, TRUE);
    if (hBtnUsbRefresh_) MoveWindow(hBtnUsbRefresh_, 10, h - 50, leftW - 20, 35, TRUE);
    if (hUsbStatus_)     MoveWindow(hUsbStatus_, rightX, usbTopY, rightW, 65, TRUE);
    if (hUsbHealthBar_)  MoveWindow(hUsbHealthBar_, rightX, usbTopY + 70, rightW, 30, TRUE);
    if (hUsbDetailList_) MoveWindow(hUsbDetailList_, rightX, usbTopY + 110, rightW, h - usbTopY - 180, TRUE);
    if (hBtnDiagnose_)   MoveWindow(hBtnDiagnose_, rightX, h - 55, 200, 40, TRUE);
    if (hBtnUsbRecover_) MoveWindow(hBtnUsbRecover_, rightX + 220, h - 55, 260, 40, TRUE);
}

// ═══════════════════════════════════════════════════════════════════════════
//  RECOVERY VIEW - Drive Panel
// ═══════════════════════════════════════════════════════════════════════════

void MainWindow::PopulateDrives() {
    SendMessage(hDriveList_, LB_RESETCONTENT, 0, 0);
    drives_ = EnumerateDrives();

    for (auto& d : drives_) {
        std::wstring display = d.label + L" (" + d.path + L")";
        SendMessage(hDriveList_, LB_ADDSTRING, 0, (LPARAM)display.c_str());
    }

    SetWindowTextW(hStatusLabel_,
        (std::to_wstring(drives_.size()) + L" unidades encontradas. Selecciona una y escanea.").c_str());
}

void MainWindow::DrawDriveItem(DRAWITEMSTRUCT* dis) {
    if (dis->itemID == (UINT)-1) return;

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH bg = selected ? hBrushAccent_ : hBrushPanel_;
    FillRect(hdc, &rc, bg);

    if (dis->itemID >= drives_.size()) return;
    auto& drive = drives_[dis->itemID];

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT);

    // Drive icon
    SelectObject(hdc, hFontTitle_);
    const wchar_t* icon = L"HDD";
    if (drive.driveType == DRIVE_REMOVABLE) icon = L"USB";
    else if (drive.driveType == DRIVE_CDROM) icon = L"CD";

    RECT iconRc = { rc.left + 8, rc.top + 10, rc.left + 55, rc.bottom - 10 };
    DrawTextW(hdc, icon, -1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Label
    SelectObject(hdc, hFontBold_);
    RECT nameRc = { rc.left + 60, rc.top + 8, rc.right - 8, rc.top + 26 };
    DrawTextW(hdc, drive.label.c_str(), -1, &nameRc, DT_LEFT | DT_SINGLELINE);

    // Info
    SetTextColor(hdc, CLR_TEXT_DIM);
    SelectObject(hdc, hFontSmall_);
    std::wstring info = drive.path + L"  |  " + drive.fileSystem;
    RECT infoRc = { rc.left + 60, rc.top + 28, rc.right - 8, rc.top + 42 };
    DrawTextW(hdc, info.c_str(), -1, &infoRc, DT_LEFT | DT_SINGLELINE);

    // Usage bar
    if (drive.totalBytes > 0) {
        double usedPct = 1.0 - (double)drive.freeBytes / drive.totalBytes;
        int barW = rc.right - rc.left - 68;
        int barX = rc.left + 60;
        int barY = rc.top + 48;

        RECT barBg = { barX, barY, barX + barW, barY + 8 };
        FillRect(hdc, &barBg, CreateSolidBrush(RGB(60, 60, 80)));

        int usedW = (int)(barW * usedPct);
        RECT barFg = { barX, barY, barX + usedW, barY + 8 };
        COLORREF barColor = usedPct > 0.9 ? CLR_RED :
                            usedPct > 0.7 ? CLR_YELLOW : CLR_ACCENT;
        FillRect(hdc, &barFg, CreateSolidBrush(barColor));

        std::wstring sizeText = FormatFileSize(drive.totalBytes - drive.freeBytes) +
                                L" / " + FormatFileSize(drive.totalBytes);
        RECT sizeRc = { barX, barY + 10, barX + barW, barY + 24 };
        DrawTextW(hdc, sizeText.c_str(), -1, &sizeRc, DT_LEFT | DT_SINGLELINE);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  RECOVERY VIEW - Scanning
// ═══════════════════════════════════════════════════════════════════════════

void MainWindow::StartScan(ScanType type) {
    int sel = (int)SendMessage(hDriveList_, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)drives_.size()) {
        MessageBoxW(hWnd_, L"Selecciona una unidad primero.", L"Recovery Total", MB_ICONWARNING);
        return;
    }

    selectedDrive_ = sel;
    allResults_.clear();
    filteredResults_.clear();
    ListView_SetItemCountEx(hFileList_, 0, 0);

    EnableWindow(hBtnQuickScan_, FALSE);
    EnableWindow(hBtnDeepScan_, FALSE);
    EnableWindow(hBtnStop_, TRUE);

    // Update step indicator
    SetWindowTextW(hStepLabel_,
        L"  PASO 1: Selecciona unidad  [OK]   |   >> PASO 2: Escaneando...   |   PASO 3: Recuperar");

    HWND hwnd = hWnd_;
    scanner_.StartScan(drives_[sel], type, [hwnd](const ScanProgress&) {
        PostMessage(hwnd, WM_SCAN_PROGRESS, 0, 0);
    });
}

void MainWindow::StopScan() {
    scanner_.StopScan();
    EnableWindow(hBtnQuickScan_, TRUE);
    EnableWindow(hBtnDeepScan_, TRUE);
    EnableWindow(hBtnStop_, FALSE);
    SetWindowTextW(hStatusLabel_, L"Escaneo detenido por el usuario.");
    SetWindowTextW(hStepLabel_,
        L"  PASO 1: Selecciona unidad   |   PASO 2: Escanear   |   PASO 3: Recuperar");
}

void MainWindow::OnScanProgress() {
    auto progress = scanner_.GetProgress();

    int pos = (int)(progress.progressPct * 10);
    SendMessage(hProgress_, PBM_SETPOS, pos, 0);

    std::wstring status = progress.currentAction +
                          L"  |  " + std::to_wstring(progress.filesFound) + L" archivos encontrados";
    SetWindowTextW(hStatusLabel_, status.c_str());

    if (!scanner_.IsScanning()) {
        OnScanComplete();
    }
}

void MainWindow::OnScanComplete() {
    allResults_ = scanner_.GetResults();
    ApplyFilter();

    EnableWindow(hBtnQuickScan_, TRUE);
    EnableWindow(hBtnDeepScan_, TRUE);
    EnableWindow(hBtnStop_, FALSE);

    SendMessage(hProgress_, PBM_SETPOS, 1000, 0);

    std::wstring status = L"Escaneo completo. " +
                          std::to_wstring(allResults_.size()) + L" archivos recuperables encontrados.";
    SetWindowTextW(hStatusLabel_, status.c_str());

    // Update step indicator
    SetWindowTextW(hStepLabel_,
        L"  PASO 1: Selecciona unidad  [OK]   |   PASO 2: Escanear  [OK]   |   >> PASO 3: Recuperar");

    uint64_t totalSize = 0;
    for (auto& f : allResults_) totalSize += f.fileSize;
    std::wstring stats = std::to_wstring(allResults_.size()) + L" archivos | " +
                         FormatFileSize(totalSize);
    SetWindowTextW(hStatsLabel_, stats.c_str());
}

// ═══════════════════════════════════════════════════════════════════════════
//  RECOVERY VIEW - File List
// ═══════════════════════════════════════════════════════════════════════════

void MainWindow::ApplyFilter() {
    filteredResults_.clear();

    for (auto& f : allResults_) {
        if (currentFilter_ == L"Todos" || currentFilter_ == L"All") {
            filteredResults_.push_back(f);
            continue;
        }

        std::wstring ext = f.extension;
        bool match = false;

        if (currentFilter_ == L"Imagenes" || currentFilter_ == L"Image")
            match = (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".gif" ||
                     ext == L".bmp" || ext == L".tif" || ext == L".webp");
        else if (currentFilter_ == L"Documentos" || currentFilter_ == L"Document")
            match = (ext == L".pdf" || ext == L".doc" || ext == L".docx" || ext == L".xls" ||
                     ext == L".xlsx" || ext == L".ppt" || ext == L".pptx" || ext == L".txt");
        else if (currentFilter_ == L"Videos" || currentFilter_ == L"Video")
            match = (ext == L".mp4" || ext == L".avi" || ext == L".mkv" || ext == L".mov" || ext == L".wmv");
        else if (currentFilter_ == L"Audio")
            match = (ext == L".mp3" || ext == L".wav" || ext == L".flac" || ext == L".ogg" || ext == L".wma");
        else if (currentFilter_ == L"Archivos" || currentFilter_ == L"Archive")
            match = (ext == L".zip" || ext == L".rar" || ext == L".7z" || ext == L".gz" || ext == L".tar");
        else if (currentFilter_ == L"Programas" || currentFilter_ == L"Program")
            match = (ext == L".exe" || ext == L".dll" || ext == L".sys");

        if (match) filteredResults_.push_back(f);
    }

    ListView_SetItemCountEx(hFileList_, (int)filteredResults_.size(), LVSICF_NOINVALIDATEALL);
    InvalidateRect(hFileList_, nullptr, TRUE);
}

void MainWindow::SelectAllFiles(bool select) {
    allSelected_ = select;
    for (auto& f : filteredResults_)
        f.selected = select;
    InvalidateRect(hFileList_, nullptr, TRUE);
    SetWindowTextW(hBtnSelectAll_, select ? L"Deseleccionar todo" : L"Seleccionar todo");
}

void MainWindow::PopulateFileList() {
    ApplyFilter();
}

// ═══════════════════════════════════════════════════════════════════════════
//  RECOVERY VIEW - Recovery
// ═══════════════════════════════════════════════════════════════════════════

std::wstring MainWindow::BrowseForFolder() {
    wchar_t path[MAX_PATH] = {};
    BROWSEINFOW bi = {};
    bi.hwndOwner = hWnd_;
    bi.lpszTitle = L"Selecciona carpeta destino para los archivos recuperados:";
    bi.ulFlags   = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl && SHGetPathFromIDListW(pidl, path)) {
        CoTaskMemFree(pidl);
        return path;
    }
    if (pidl) CoTaskMemFree(pidl);
    return L"";
}

void MainWindow::RecoverSelected() {
    std::vector<RecoveredFile> selected;
    for (auto& f : filteredResults_) {
        if (f.selected && f.status != FileStatus::Unrecoverable)
            selected.push_back(f);
    }

    if (selected.empty()) {
        MessageBoxW(hWnd_,
            L"No hay archivos seleccionados.\n\nHaz clic en los archivos de la lista para seleccionarlos.",
            L"Recovery Total", MB_ICONINFORMATION);
        return;
    }

    std::wstring destDir = BrowseForFolder();
    if (destDir.empty()) return;

    // Warn about same drive
    if (selectedDrive_ >= 0 && selectedDrive_ < (int)drives_.size()) {
        auto& srcDrive = drives_[selectedDrive_];
        if (!srcDrive.isPhysical && destDir.substr(0, 2) == srcDrive.path.substr(0, 2)) {
            int res = MessageBoxW(hWnd_,
                L"ADVERTENCIA: Guardar archivos recuperados en la misma unidad reduce las posibilidades de exito.\n\n"
                L"Elige una unidad diferente para mejores resultados.\n\nContinuar de todos modos?",
                L"Recovery Total", MB_YESNO | MB_ICONWARNING);
            if (res != IDYES) return;
        }
    }

    SetWindowTextW(hStatusLabel_, L"Recuperando archivos...");
    EnableWindow(hBtnRecover_, FALSE);

    std::wstring sourcePath = drives_[selectedDrive_].path;
    HWND hwnd = hWnd_;

    bool success = Recovery::RecoverFiles(selected, sourcePath, destDir,
        [hwnd](uint32_t current, uint32_t total, const std::wstring& name) {
            std::wstring msg = L"Recuperando " + std::to_wstring(current) +
                               L"/" + std::to_wstring(total) + L": " + name;
            SetWindowTextW(GetDlgItem(hwnd, IDC_STATUS_LABEL), msg.c_str());

            int pos = (int)(1000.0 * current / total);
            SendMessage(GetDlgItem(hwnd, IDC_PROGRESS), PBM_SETPOS, pos, 0);
        });

    EnableWindow(hBtnRecover_, TRUE);

    if (success) {
        std::wstring msg = L"Recuperacion completada!\n\nArchivos guardados en:\n" + destDir;
        MessageBoxW(hWnd_, msg.c_str(), L"Recovery Total", MB_ICONINFORMATION);
        ShellExecuteW(nullptr, L"open", destDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        MessageBoxW(hWnd_,
            L"La recuperacion fallo. Asegurate de tener privilegios de administrador\n"
            L"y de que la unidad destino tiene suficiente espacio.",
            L"Recovery Total", MB_ICONERROR);
    }

    SetWindowTextW(hStatusLabel_, L"Recuperacion completada.");
}

// ═══════════════════════════════════════════════════════════════════════════
//  USB DIAGNOSTIC VIEW
// ═══════════════════════════════════════════════════════════════════════════

void MainWindow::RefreshUsbList() {
    SendMessage(hUsbList_, LB_RESETCONTENT, 0, 0);
    usbDrives_ = EnumerateUsbDrives();

    for (auto& d : usbDrives_) {
        std::wstring display = d.friendlyName;
        SendMessage(hUsbList_, LB_ADDSTRING, 0, (LPARAM)display.c_str());
    }

    if (usbDrives_.empty()) {
        SetWindowTextW(hUsbStatus_,
            L"  No se encontraron dispositivos USB.\n"
            L"  Conecta un USB y pulsa \"Actualizar dispositivos USB\".");
    } else {
        SetWindowTextW(hUsbStatus_,
            (L"  " + std::to_wstring(usbDrives_.size()) +
             L" dispositivo(s) USB detectado(s).\n"
             L"  Selecciona uno y pulsa \"DIAGNOSTICAR\".").c_str());
    }

    // Clear detail list
    ListView_DeleteAllItems(hUsbDetailList_);
    SendMessage(hUsbHealthBar_, PBM_SETPOS, 0, 0);
    selectedUsb_ = -1;
}

void MainWindow::DrawUsbItem(DRAWITEMSTRUCT* dis) {
    if (dis->itemID == (UINT)-1) return;

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH bg = selected ? hBrushAccent_ : hBrushPanel_;
    FillRect(hdc, &rc, bg);

    if (dis->itemID >= usbDrives_.size()) return;
    auto& usb = usbDrives_[dis->itemID];

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT);

    // USB icon
    SelectObject(hdc, hFontTitle_);
    RECT iconRc = { rc.left + 8, rc.top + 5, rc.left + 55, rc.top + 35 };
    DrawTextW(hdc, L"USB", -1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Name
    SelectObject(hdc, hFontBold_);
    RECT nameRc = { rc.left + 60, rc.top + 6, rc.right - 8, rc.top + 24 };
    DrawTextW(hdc, usb.friendlyName.c_str(), -1, &nameRc, DT_LEFT | DT_SINGLELINE);

    // Manufacturer + product
    SetTextColor(hdc, CLR_TEXT_DIM);
    SelectObject(hdc, hFontSmall_);
    std::wstring details;
    if (!usb.manufacturer.empty()) details = usb.manufacturer;
    if (!usb.productId.empty()) {
        if (!details.empty()) details += L" - ";
        details += usb.productId;
    }
    if (details.empty()) details = L"Dispositivo USB";
    RECT detRc = { rc.left + 60, rc.top + 26, rc.right - 8, rc.top + 40 };
    DrawTextW(hdc, details.c_str(), -1, &detRc, DT_LEFT | DT_SINGLELINE);

    // Size + FS
    std::wstring sizeInfo = usb.fileSystem + L"  |  " + FormatFileSize(usb.totalBytes);
    RECT sizeRc = { rc.left + 60, rc.top + 42, rc.right - 8, rc.top + 56 };
    DrawTextW(hdc, sizeInfo.c_str(), -1, &sizeRc, DT_LEFT | DT_SINGLELINE);

    // Usage bar
    if (usb.totalBytes > 0) {
        double usedPct = 1.0 - (double)usb.freeBytes / usb.totalBytes;
        int barW = rc.right - rc.left - 68;
        int barX = rc.left + 60;
        int barY = rc.top + 60;

        RECT barBg = { barX, barY, barX + barW, barY + 8 };
        FillRect(hdc, &barBg, CreateSolidBrush(RGB(60, 60, 80)));

        int usedW = (int)(barW * usedPct);
        RECT barFg = { barX, barY, barX + usedW, barY + 8 };
        COLORREF barColor = usedPct > 0.9 ? CLR_RED :
                            usedPct > 0.7 ? CLR_YELLOW : CLR_ACCENT2;
        FillRect(hdc, &barFg, CreateSolidBrush(barColor));
    }
}

void MainWindow::ShowUsbDeviceInfo(int index) {
    if (index < 0 || index >= (int)usbDrives_.size()) return;
    auto& usb = usbDrives_[index];

    // Show basic info in detail list
    ListView_DeleteAllItems(hUsbDetailList_);

    auto addRow = [&](const wchar_t* prop, const std::wstring& val) {
        int idx = ListView_GetItemCount(hUsbDetailList_);
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = idx;
        item.pszText = const_cast<wchar_t*>(prop);
        ListView_InsertItem(hUsbDetailList_, &item);
        ListView_SetItemText(hUsbDetailList_, idx, 1, const_cast<wchar_t*>(val.c_str()));
    };

    addRow(L"Nombre", usb.friendlyName);
    addRow(L"Letra de unidad", usb.driveLetter);
    addRow(L"Fabricante", usb.manufacturer.empty() ? L"Desconocido" : usb.manufacturer);
    addRow(L"Producto", usb.productId.empty() ? L"Desconocido" : usb.productId);
    addRow(L"Numero de serie", usb.serialNumber.empty() ? L"No disponible" : usb.serialNumber);
    addRow(L"Tipo de bus", usb.busType);
    addRow(L"Tipo de medio", usb.mediaType);
    addRow(L"Sistema de archivos", usb.fileSystem);
    addRow(L"Capacidad total", FormatFileSize(usb.totalBytes));
    addRow(L"Espacio libre", FormatFileSize(usb.freeBytes));
    addRow(L"Espacio usado", FormatFileSize(usb.totalBytes - usb.freeBytes));
    addRow(L"Protegido contra escritura", usb.isWriteProtected ? L"Si" : L"No");

    if (usb.totalBytes > 0) {
        double usedPct = 100.0 * (1.0 - (double)usb.freeBytes / usb.totalBytes);
        std::wostringstream oss;
        oss << std::fixed << std::setprecision(1) << usedPct << L"%";
        addRow(L"Porcentaje usado", oss.str());
    }

    SetWindowTextW(hUsbStatus_,
        (L"  Dispositivo: " + usb.friendlyName + L"\n"
         L"  Pulsa \"DIAGNOSTICAR\" para un analisis completo del estado.").c_str());
}

void MainWindow::RunDiagnostic() {
    if (selectedUsb_ < 0 || selectedUsb_ >= (int)usbDrives_.size()) {
        MessageBoxW(hWnd_, L"Selecciona un dispositivo USB de la lista primero.",
                    L"Recovery Total", MB_ICONWARNING);
        return;
    }

    auto& device = usbDrives_[selectedUsb_];

    SetWindowTextW(hUsbStatus_,
        (L"  Diagnosticando: " + device.friendlyName + L"\n"
         L"  Analizando estado del dispositivo... Por favor espera.").c_str());
    EnableWindow(hBtnDiagnose_, FALSE);

    // Run diagnostic
    lastDiagResult_ = DiagnoseUsbDrive(device);

    EnableWindow(hBtnDiagnose_, TRUE);

    DisplayDiagnosticResult(device, lastDiagResult_);
}

void MainWindow::DisplayDiagnosticResult(const UsbDeviceInfo& device,
                                          const DiagnosticResult& result) {
    // Update health bar
    SendMessage(hUsbHealthBar_, PBM_SETPOS, result.healthScore, 0);

    COLORREF barColor = HealthLevelToColor(result.overallHealth);
    SendMessage(hUsbHealthBar_, PBM_SETBARCOLOR, 0, barColor);

    // Update status text
    std::wstring healthText = HealthLevelToString(result.overallHealth);
    std::wstring statusText =
        L"  " + device.friendlyName + L"  -  Salud: " +
        healthText + L" (" + std::to_wstring(result.healthScore) + L"/100)\n";

    if (result.warnings.empty()) {
        statusText += L"  El dispositivo esta en buen estado.";
    } else {
        statusText += L"  Se encontraron " + std::to_wstring(result.warnings.size()) + L" problema(s).";
    }

    SetWindowTextW(hUsbStatus_, statusText.c_str());

    // Populate detail list
    ListView_DeleteAllItems(hUsbDetailList_);

    auto addRow = [&](const wchar_t* prop, const std::wstring& val) {
        int idx = ListView_GetItemCount(hUsbDetailList_);
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = idx;
        item.pszText = const_cast<wchar_t*>(prop);
        ListView_InsertItem(hUsbDetailList_, &item);
        ListView_SetItemText(hUsbDetailList_, idx, 1, const_cast<wchar_t*>(val.c_str()));
    };

    // === Device info section ===
    addRow(L"--- INFORMACION DEL DISPOSITIVO ---", L"");
    addRow(L"Dispositivo", device.friendlyName);
    addRow(L"Fabricante", device.manufacturer.empty() ? L"Desconocido" : device.manufacturer);
    addRow(L"Producto", device.productId.empty() ? L"Desconocido" : device.productId);
    addRow(L"Numero de serie", device.serialNumber.empty() ? L"No disponible" : device.serialNumber);
    addRow(L"Sistema de archivos", device.fileSystem);
    addRow(L"Capacidad", FormatFileSize(device.totalBytes));
    addRow(L"Espacio libre", FormatFileSize(device.freeBytes));

    // === Health section ===
    addRow(L"", L"");
    addRow(L"--- RESULTADO DEL DIAGNOSTICO ---", L"");
    addRow(L"Puntuacion de salud", std::to_wstring(result.healthScore) + L" / 100");
    addRow(L"Estado general", healthText);
    addRow(L"Accesible", result.isAccessible ? L"Si" : L"No");
    addRow(L"Sistema de archivos OK", result.fileSystemOk ? L"Si" : L"No - Posiblemente danado");
    addRow(L"Espacio libre OK", result.freeSpaceOk ? L"Si" : L"Casi lleno");
    addRow(L"Test de lectura", result.readTestPassed ? L"Pasado" : L"Fallido");

    // Speed
    if (result.seqReadSpeed > 0) {
        std::wostringstream oss;
        oss << std::fixed << std::setprecision(1) << result.seqReadSpeed << L" MB/s";
        std::wstring speedDesc;
        if (result.seqReadSpeed >= 100) speedDesc = L" (USB 3.0+)";
        else if (result.seqReadSpeed >= 25) speedDesc = L" (USB 2.0 rapido)";
        else if (result.seqReadSpeed >= 5) speedDesc = L" (USB 2.0 normal)";
        else speedDesc = L" (Muy lento - posible problema)";
        addRow(L"Velocidad de lectura", oss.str() + speedDesc);
    }

    addRow(L"Protegido contra escritura", device.isWriteProtected ? L"Si" : L"No");

    // === Warnings section ===
    if (!result.warnings.empty()) {
        addRow(L"", L"");
        addRow(L"--- AVISOS ---", L"");
        for (size_t i = 0; i < result.warnings.size(); ++i) {
            std::wstring label = L"Aviso " + std::to_wstring(i + 1);
            addRow(label.c_str(), result.warnings[i]);
        }
    }

    // === Recommendations section ===
    if (!result.recommendations.empty()) {
        addRow(L"", L"");
        addRow(L"--- RECOMENDACIONES ---", L"");
        for (size_t i = 0; i < result.recommendations.size(); ++i) {
            std::wstring label = std::to_wstring(i + 1) + L".";
            addRow(label.c_str(), result.recommendations[i]);
        }
    }
}

void MainWindow::RecoverFromUsb() {
    if (selectedUsb_ < 0 || selectedUsb_ >= (int)usbDrives_.size()) {
        MessageBoxW(hWnd_, L"Selecciona un dispositivo USB primero.",
                    L"Recovery Total", MB_ICONWARNING);
        return;
    }

    auto& usb = usbDrives_[selectedUsb_];

    // Find the matching drive in the recovery drives_ list, or add it
    SwitchView(AppView::Recovery);

    // Select the matching drive
    for (int i = 0; i < (int)drives_.size(); ++i) {
        if (drives_[i].path == usb.driveLetter) {
            SendMessage(hDriveList_, LB_SETCURSEL, i, 0);
            selectedDrive_ = i;
            SetWindowTextW(hStatusLabel_,
                (L"Unidad USB seleccionada: " + usb.friendlyName +
                 L". Pulsa Escanear para buscar archivos.").c_str());
            break;
        }
    }
}

} // namespace rt
