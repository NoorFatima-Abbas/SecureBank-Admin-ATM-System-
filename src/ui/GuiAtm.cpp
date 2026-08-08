/**
 * @file GuiAtm.cpp
 * @brief Custom Win32 ATM + Admin GUI for SecureBank — every pixel hand-crafted.
 * @details Uses only Windows built-in GDI/User32 APIs. No external libraries.
 * Dark navy theme with SecureBank gold branding, owner-drawn buttons, and
 * custom-coloured input fields. All financial logic delegates to BankSystem.
 *
 * Portal screen → Customer ATM  |  Admin Console (password: 1234)
 *
 * Admin operations : Create · View All · Search · Unlock · Delete
 * ATM  operations  : Balance · Withdraw · Deposit · Transfer · Mini-Statement
 *
 * Duplicate account numbers are prevented automatically by BankSystem's
 * monotonically-increasing account-number counter.
 *
 * Owned by: Ayesha Kamran
 */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "core/BankSystem.hpp"
#include "domain/Enums.hpp"
#include "persistence/TransactionJournal.hpp"
#include "security/PinHasher.hpp"
#include "ui/GuiAtm.hpp"
#include "ui/OtpService.hpp"
#include "ui/ReceiptWriter.hpp"

namespace securebank::ui {

// ============================================================
//  COLOUR PALETTE
// ============================================================
namespace clr {
    constexpr COLORREF bg       = RGB( 11,  22,  35);
    constexpr COLORREF brand    = RGB(240, 165,   0);
    constexpr COLORREF success  = RGB( 39, 174,  96);
    constexpr COLORREF danger   = RGB(231,  76,  60);
    constexpr COLORREF action   = RGB( 41, 128, 185);
    constexpr COLORREF btnMenu  = RGB( 28,  60,  90);
    constexpr COLORREF btnBack  = RGB( 44,  72, 100);
    constexpr COLORREF adminBtn = RGB( 90,  55,   5);
    constexpr COLORREF adminAct = RGB( 30, 100,  60);
    constexpr COLORREF white    = RGB(255, 255, 255);
    constexpr COLORREF muted    = RGB(149, 165, 166);
    constexpr COLORREF inputBg  = RGB( 16,  32,  50);
    constexpr COLORREF listBg   = RGB( 14,  28,  44);
    constexpr COLORREF listSel  = RGB( 41, 128, 185);
}

// ============================================================
//  CONTROL IDs
// ============================================================
enum : int {
    // --- ATM buttons ---
    BTN_LOGIN           = 101,
    BTN_BACK            = 102,
    BTN_BALANCE         = 103,
    BTN_WITHDRAW        = 104,
    BTN_DEPOSIT         = 105,
    BTN_TRANSFER        = 106,
    BTN_MINI            = 107,
    BTN_LOGOUT          = 108,
    BTN_CONFIRM         = 109,
    BTN_CANCEL          = 110,
    BTN_OTP_VERIFY      = 111,

    // --- Portal ---
    BTN_USER_MODE       = 112,
    BTN_ADMIN_MODE      = 113,

    // --- Admin navigation ---
    BTN_ADMIN_LOGIN     = 114,
    BTN_TO_CREATE       = 115,
    BTN_TO_UNLOCK       = 116,
    BTN_TO_DELETE       = 117,
    BTN_ADMIN_LOGOUT    = 118,
    BTN_TO_VIEW         = 119,
    BTN_TO_SEARCH       = 120,

    // --- Admin operation confirms ---
    BTN_ADMIN_CREATE_OP = 121,
    BTN_ADMIN_UNLOCK_OP = 122,
    BTN_ADMIN_DELETE_OP = 123,
    BTN_ADMIN_SEARCH_OP = 124,

    // --- Account type toggles ---
    BTN_TYPE_SAVINGS    = 125,
    BTN_TYPE_CURRENT    = 126,

    // --- Edit controls ---
    EDT_ACCOUNT         = 201,
    EDT_PIN             = 202,
    EDT_AMOUNT          = 203,
    EDT_TO_ACCOUNT      = 204,
    EDT_OTP             = 205,
    EDT_ADMIN_PASS      = 206,
    EDT_NAME            = 207,
    EDT_CNIC            = 208,
    EDT_PHONE           = 209,
    EDT_NEW_PIN         = 210,
    EDT_OPEN_BAL        = 211,
    EDT_TARGET_ACCT     = 212,
    EDT_SEARCH_TERM     = 213,

    // --- List box ---
    LBX_ACCOUNTS        = 401,

    // --- Statics ---
    STC_TITLE           = 301,
    STC_ERROR           = 302,
    STC_BALANCE         = 303,
    STC_OTP_CODE        = 304,
    STC_WELCOME         = 305,
    STC_ADMIN_INFO      = 306,
    STC_LIST_HDR        = 307,
};

// ============================================================
//  SCREEN ENUM
// ============================================================
enum class Screen {
    Portal,
    // ATM
    Login, Dashboard, Balance,
    Withdraw, Deposit, Transfer,
    OtpVerify, MiniStatement,
    // Admin
    AdminLogin, AdminDashboard,
    AdminCreate, AdminView, AdminSearch,
    AdminUnlock, AdminDelete,
    // Shared results
    Success, Error
};

// ============================================================
//  MODULE GLOBALS
// ============================================================
namespace g {
    core::BankSystem*       bank          = nullptr;
    Screen                  screen        = Screen::Portal;
    bool                    isAdminMode   = false;

    // ATM state
    long                    acctNo        = 0;
    std::string             name;
    double                  pendingAmt    = 0.0;
    long                    pendingToAcct = 0;
    domain::TransactionType pendingType   = domain::TransactionType::Withdrawal;
    OtpService              otp;

    // Admin create state
    domain::AccountType     newAcctType   = domain::AccountType::Savings;

    // Result messages
    std::string             successMsg;
    std::string             errorMsg;

    // GDI resources
    HFONT   fontTitle  = nullptr;
    HFONT   fontSub    = nullptr;
    HFONT   fontLabel  = nullptr;
    HFONT   fontInput  = nullptr;
    HFONT   fontBtn    = nullptr;
    HFONT   fontMono   = nullptr;   // Consolas — used in account lists
    HBRUSH  brBg       = nullptr;
    HBRUSH  brInput    = nullptr;
    HBRUSH  brList     = nullptr;
}

// ============================================================
//  SMALL HELPERS
// ============================================================
static std::string fmtAmt(double v) {
    std::ostringstream o;
    o << "Rs. " << std::fixed << std::setprecision(2) << v;
    return o.str();
}

// Pad or truncate a string to exactly `w` chars
static std::string pad(std::string_view s, int w) {
    std::string r(s);
    if (static_cast<int>(r.size()) > w) r.resize(static_cast<size_t>(w));
    while (static_cast<int>(r.size()) < w) r += ' ';
    return r;
}

// Format one account into a fixed-width list row
static std::string fmtAcctRow(const domain::Account& a) {
    std::string type   = (a.type()   == domain::AccountType::Savings)    ? "Savings" : "Current";
    std::string status;
    switch (a.status()) {
        case domain::AccountStatus::Active: status = "Active";  break;
        case domain::AccountStatus::Locked: status = "Locked";  break;
        case domain::AccountStatus::Closed: status = "Closed";  break;
    }
    std::ostringstream o;
    o << pad(std::to_string(a.accountNumber()), 7)
      << "  " << pad(a.customerName(), 24)
      << "  " << pad(type, 9)
      << "  " << pad(status, 8)
      << "  " << fmtAmt(a.balance());
    return o.str();
}

static std::string getEditText(HWND hwnd, int id) {
    HWND h = GetDlgItem(hwnd, id);
    if (!h) return "";
    int len = GetWindowTextLengthA(h);
    if (len <= 0) return "";
    std::string buf(static_cast<size_t>(len + 1), '\0');
    GetWindowTextA(h, buf.data(), len + 1);
    buf.resize(static_cast<size_t>(len));
    return buf;
}

static void setError(HWND hwnd, const char* msg) {
    HWND h = GetDlgItem(hwnd, STC_ERROR);
    if (h) SetWindowTextA(h, msg);
}

static void clearAllChildren(HWND hwnd) {
    std::vector<HWND> kids;
    EnumChildWindows(hwnd, [](HWND c, LPARAM lp) -> BOOL {
        reinterpret_cast<std::vector<HWND>*>(lp)->push_back(c);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&kids));
    for (HWND k : kids) DestroyWindow(k);
}

// ============================================================
//  CONTROL FACTORIES
// ============================================================
static HWND makeEdit(HWND p, int id, int x, int y, int w, int h,
                     bool pwd = false, const char* init = "") {
    DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    if (pwd) style |= ES_PASSWORD;
    HWND e = CreateWindowA("EDIT", init, style,
        x, y, w, h, p, (HMENU)(intptr_t)id,
        GetModuleHandleA(nullptr), nullptr);
    SendMessageA(e, WM_SETFONT, (WPARAM)g::fontInput, TRUE);
    if (pwd) SendMessageA(e, EM_SETPASSWORDCHAR, (WPARAM)'*', 0);
    return e;
}

static HWND makeBtn(HWND p, int id, const char* text,
                    int x, int y, int w, int h) {
    HWND b = CreateWindowA("BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, w, h, p, (HMENU)(intptr_t)id,
        GetModuleHandleA(nullptr), nullptr);
    SendMessageA(b, WM_SETFONT, (WPARAM)g::fontBtn, TRUE);
    return b;
}

static HWND makeLabel(HWND p, int id, const char* text,
                      int x, int y, int w, int h,
                      HFONT font = nullptr, DWORD extra = SS_LEFT) {
    HWND s = CreateWindowA("STATIC", text,
        WS_CHILD | WS_VISIBLE | extra,
        x, y, w, h, p, (HMENU)(intptr_t)id,
        GetModuleHandleA(nullptr), nullptr);
    SendMessageA(s, WM_SETFONT, (WPARAM)(font ? font : g::fontLabel), TRUE);
    return s;
}

// Monospace list-box (scrollable, dark-themed)
static HWND makeListBox(HWND p, int id, int x, int y, int w, int h) {
    HWND lb = CreateWindowA("LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_NOSEL,
        x, y, w, h, p, (HMENU)(intptr_t)id,
        GetModuleHandleA(nullptr), nullptr);
    SendMessageA(lb, WM_SETFONT, (WPARAM)g::fontMono, TRUE);
    return lb;
}

// ============================================================
//  OWNER-DRAW BUTTON PAINTER
// ============================================================
static void paintButton(LPDRAWITEMSTRUCT di) {
    COLORREF fill;
    switch (di->CtlID) {
        case BTN_LOGIN:
        case BTN_CONFIRM:
        case BTN_OTP_VERIFY:
        case BTN_ADMIN_LOGIN:  fill = clr::action;    break;

        case BTN_CANCEL:
        case BTN_LOGOUT:       fill = clr::danger;    break;

        case BTN_BACK:         fill = clr::btnBack;   break;

        case BTN_USER_MODE:    fill = clr::action;    break;
        case BTN_ADMIN_MODE:   fill = clr::adminBtn;  break;

        case BTN_TO_CREATE:
        case BTN_TO_VIEW:
        case BTN_TO_SEARCH:
        case BTN_TO_UNLOCK:
        case BTN_TO_DELETE:    fill = clr::btnMenu;   break;

        case BTN_ADMIN_LOGOUT: fill = clr::danger;    break;

        case BTN_ADMIN_CREATE_OP:
        case BTN_ADMIN_UNLOCK_OP:
        case BTN_ADMIN_DELETE_OP:
        case BTN_ADMIN_SEARCH_OP: fill = clr::adminAct; break;

        case BTN_TYPE_SAVINGS:
            fill = (g::newAcctType == domain::AccountType::Savings)
                   ? clr::brand : clr::btnMenu;
            break;
        case BTN_TYPE_CURRENT:
            fill = (g::newAcctType == domain::AccountType::Current)
                   ? clr::brand : clr::btnMenu;
            break;

        default: fill = clr::btnMenu; break;
    }

    if (di->itemState & ODS_SELECTED) {
        fill = RGB(
            static_cast<BYTE>(GetRValue(fill) * 75 / 100),
            static_cast<BYTE>(GetGValue(fill) * 75 / 100),
            static_cast<BYTE>(GetBValue(fill) * 75 / 100));
    }

    HDC  dc = di->hDC;
    RECT rc = di->rcItem;

    HBRUSH br  = CreateSolidBrush(fill);
    HPEN   pen = CreatePen(PS_NULL, 0, 0);
    SelectObject(dc, br);
    SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
    DeleteObject(SelectObject(dc, GetStockObject(NULL_BRUSH)));
    DeleteObject(pen);

    char buf[128] = {};
    GetWindowTextA(di->hwndItem, buf, sizeof(buf));
    SetBkMode(dc, TRANSPARENT);

    bool goldSelected =
        (di->CtlID == BTN_TYPE_SAVINGS && g::newAcctType == domain::AccountType::Savings) ||
        (di->CtlID == BTN_TYPE_CURRENT && g::newAcctType == domain::AccountType::Current);
    SetTextColor(dc, goldSelected ? RGB(20, 20, 20) : clr::white);

    HFONT of = (HFONT)SelectObject(dc, g::fontBtn);
    DrawTextA(dc, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

// ============================================================
//  BACKGROUND PAINTER
// ============================================================
static void paintBackground(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);

    RECT cl;
    GetClientRect(hwnd, &cl);
    FillRect(dc, &cl, g::brBg);

    // Gold left-edge accent
    HBRUSH bar = CreateSolidBrush(clr::brand);
    RECT barRc = {0, 0, 5, cl.bottom};
    FillRect(dc, &barRc, bar);
    DeleteObject(bar);

    if (g::screen != Screen::Portal) {
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g::fontSub);
        SetTextColor(dc, clr::brand);
        TextOutA(dc, 14, 6, "SecureBank", 10);
        if (g::isAdminMode) {
            HFONT of2 = (HFONT)SelectObject(dc, g::fontLabel);
            SetTextColor(dc, clr::danger);
            TextOutA(dc, 14, 30, "ADMIN SESSION", 13);
            SelectObject(dc, of2);
        }
        SelectObject(dc, of);
    }

    EndPaint(hwnd, &ps);
}

// ============================================================
//  ACCOUNT LIST HELPERS (populate a LISTBOX)
// ============================================================
static void populateListBox(HWND lb,
                            const std::vector<domain::Account>& accounts) {
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    if (accounts.empty()) {
        SendMessageA(lb, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>("  (no accounts found)"));
        return;
    }
    for (const auto& a : accounts)
        SendMessageA(lb, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(fmtAcctRow(a).c_str()));
}

static void addListHeader(HWND hw, int x, int y, int w) {
    // Fixed-width column header above the list box
    std::string hdr =
        pad("AcctNo",  7) + "  " +
        pad("Name",   24) + "  " +
        pad("Type",    9) + "  " +
        pad("Status",  8) + "  " +
        "Balance";
    makeLabel(hw, STC_LIST_HDR, hdr.c_str(),
              x, y, w, 20, g::fontMono, SS_LEFT);
}

// ============================================================
//  SCREEN BUILDERS
// ============================================================

// ---- Portal ----
static void buildPortal(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    constexpr int CX = 440;

    makeLabel(hw, STC_TITLE, "SecureBank",
              CX - 300, 90, 600, 60, g::fontTitle, SS_CENTER);
    makeLabel(hw, 0, "BANKING MANAGEMENT SYSTEM",
              CX - 300, 155, 600, 26, g::fontSub, SS_CENTER);

    CreateWindowA("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        CX - 250, 192, 500, 2,
        hw, nullptr, GetModuleHandleA(nullptr), nullptr);

    makeLabel(hw, 0, "Select your access mode to continue:",
              CX - 240, 208, 480, 26, g::fontLabel, SS_CENTER);

    constexpr int BW = 260, BH = 110, GAP = 40, BY = 250;
    constexpr int LX = CX - BW - GAP / 2;
    constexpr int RX = CX + GAP / 2;

    makeBtn(hw, BTN_USER_MODE,  "CUSTOMER\nATM",    LX, BY, BW, BH);
    makeBtn(hw, BTN_ADMIN_MODE, "ADMIN\nCONSOLE",  RX, BY, BW, BH);

    makeLabel(hw, 0, "Deposits, withdrawals, transfers",
              LX, BY + BH + 8, BW, 22, g::fontLabel, SS_CENTER);
    makeLabel(hw, 0, "Create, view, search, unlock & delete",
              RX, BY + BH + 8, BW, 22, g::fontLabel, SS_CENTER);

    makeLabel(hw, STC_ADMIN_INFO,
              "Admin access is restricted to authorised staff only.",
              CX - 300, 430, 600, 22, g::fontLabel, SS_CENTER);
}

// ---- Admin Login ----
static void buildAdminLogin(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    constexpr int PW = 360, PX = (880 - PW) / 2, PY = 120;

    makeLabel(hw, STC_TITLE, "Admin Console",
              PX, PY, PW, 46, g::fontTitle, SS_CENTER);
    makeLabel(hw, 0, "Enter admin password to continue",
              PX, PY + 50, PW, 24, g::fontSub, SS_CENTER);

    CreateWindowA("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        PX + 10, PY + 82, PW - 20, 2,
        hw, nullptr, GetModuleHandleA(nullptr), nullptr);

    makeLabel(hw, 0, "Password",
              PX + 20, PY + 100, PW - 40, 22, g::fontLabel);
    makeEdit (hw, EDT_ADMIN_PASS,
              PX + 20, PY + 124, PW - 40, 36, true);

    makeLabel(hw, STC_ERROR, "",
              PX + 20, PY + 172, PW - 40, 22, g::fontLabel, SS_CENTER);

    makeBtn(hw, BTN_ADMIN_LOGIN, "UNLOCK ADMIN",
            PX + 40, PY + 206, PW - 80, 50);
    makeBtn(hw, BTN_BACK, "< BACK",
            PX + 40, PY + 272, PW - 80, 40);

    SetFocus(GetDlgItem(hw, EDT_ADMIN_PASS));
}

// ---- Admin Dashboard (2-column grid) ----
static void buildAdminDashboard(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_WELCOME, "Admin Console  —  Account Management",
              20, 32, 840, 38, g::fontTitle);
    makeLabel(hw, 0, "Select an operation:",
              20, 76, 600, 22, g::fontLabel);

    struct ADef { int id; const char* lbl; };
    constexpr ADef ops[6] = {
        {BTN_TO_CREATE, "Create New Account"},
        {BTN_TO_VIEW,   "View All Accounts"},
        {BTN_TO_SEARCH, "Search Account"},
        {BTN_TO_UNLOCK, "Unlock Account"},
        {BTN_TO_DELETE, "Delete Account"},
        {BTN_ADMIN_LOGOUT, "Logout  (Return to Portal)"},
    };

    constexpr int BW = 400, BH = 72, GAP = 16, SX = 20, SY = 110;
    for (int i = 0; i < 6; ++i) {
        int col = i % 2;
        int row = i / 2;
        makeBtn(hw, ops[i].id, ops[i].lbl,
                SX + col * (BW + 36),
                SY + row * (BH + GAP),
                BW, BH);
    }
}

// ---- Admin — View All Accounts ----
static void buildAdminView(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "All Accounts",
              20, 28, 840, 40, g::fontTitle);

    const auto& accounts = g::bank->allAccounts();
    std::string count = std::to_string(accounts.size()) + " account(s) on file.";
    makeLabel(hw, 0, count.c_str(), 20, 74, 500, 22, g::fontSub);

    // Column header
    addListHeader(hw, 20, 104, 836);

    CreateWindowA("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        20, 126, 836, 2,
        hw, nullptr, GetModuleHandleA(nullptr), nullptr);

    // Scrollable list
    HWND lb = makeListBox(hw, LBX_ACCOUNTS, 20, 130, 836, 360);
    populateListBox(lb, accounts);

    makeBtn(hw, BTN_BACK, "BACK TO ADMIN MENU", 20, 504, 260, 48);
}

// ---- Admin — Search Account ----
static void buildAdminSearch(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "Search Account",
              20, 28, 840, 40, g::fontTitle);
    makeLabel(hw, 0, "Search by account number or customer name (partial match supported).",
              20, 74, 820, 22, g::fontSub);

    makeLabel(hw, 0, "Search term", 20, 106, 400, 22, g::fontLabel);
    makeEdit (hw, EDT_SEARCH_TERM, 20, 130, 520, 36);
    makeBtn  (hw, BTN_ADMIN_SEARCH_OP, "SEARCH",
              556, 130, 140, 36);

    makeLabel(hw, STC_ERROR, "", 20, 174, 600, 22, g::fontLabel);

    addListHeader(hw, 20, 200, 836);
    CreateWindowA("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        20, 222, 836, 2,
        hw, nullptr, GetModuleHandleA(nullptr), nullptr);

    makeListBox(hw, LBX_ACCOUNTS, 20, 226, 836, 264);

    makeBtn(hw, BTN_BACK, "BACK TO ADMIN MENU", 20, 504, 260, 48);

    SetFocus(GetDlgItem(hw, EDT_SEARCH_TERM));
}

// ---- Admin — Create Account ----
static void buildAdminCreate(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "Create New Account",
              20, 28, 840, 40, g::fontTitle);

    constexpr int LX = 20, RX = 450, FW = 380, FH = 34, LH = 22;

    makeLabel(hw, 0, "Customer Name",          LX,  86, FW, LH, g::fontLabel);
    makeEdit (hw, EDT_NAME,                    LX, 110, FW, FH);

    makeLabel(hw, 0, "CNIC  (without dashes)", LX, 152, FW, LH, g::fontLabel);
    makeEdit (hw, EDT_CNIC,                    LX, 176, FW, FH);

    makeLabel(hw, 0, "Phone Number",           LX, 218, FW, LH, g::fontLabel);
    makeEdit (hw, EDT_PHONE,                   LX, 242, FW, FH);

    makeLabel(hw, 0, "Initial PIN",            RX,  86, FW, LH, g::fontLabel);
    makeEdit (hw, EDT_NEW_PIN,                 RX, 110, FW, FH, true);

    makeLabel(hw, 0, "Opening Balance (Rs.)",  RX, 152, FW, LH, g::fontLabel);
    makeEdit (hw, EDT_OPEN_BAL,                RX, 176, FW, FH);

    makeLabel(hw, 0, "Account Type",           RX, 218, FW, LH, g::fontLabel);
    makeBtn  (hw, BTN_TYPE_SAVINGS, "Savings", RX, 242, 184, FH + 4);
    makeBtn  (hw, BTN_TYPE_CURRENT, "Current", RX + 200, 242, 176, FH + 4);

    makeLabel(hw, STC_ERROR, "", 20, 298, 820, 24, g::fontLabel, SS_CENTER);

    makeBtn(hw, BTN_ADMIN_CREATE_OP, "CREATE ACCOUNT", 20,  336, 380, 52);
    makeBtn(hw, BTN_CANCEL,          "CANCEL",         416, 336, 200, 52);

    SetFocus(GetDlgItem(hw, EDT_NAME));
}

// ---- Admin — Unlock Account ----
static void buildAdminUnlock(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    constexpr int PW = 440, PX = (880 - PW) / 2, PY = 130;

    makeLabel(hw, STC_TITLE, "Unlock Account",
              PX, PY, PW, 40, g::fontTitle, SS_CENTER);
    makeLabel(hw, 0,
              "Accounts lock automatically after 3 consecutive\nfailed PIN attempts.",
              PX, PY + 48, PW, 44, g::fontSub, SS_CENTER);

    makeLabel(hw, 0, "Account Number to Unlock",
              PX + 20, PY + 104, PW - 40, 22, g::fontLabel);
    makeEdit (hw, EDT_TARGET_ACCT,
              PX + 20, PY + 128, PW - 40, 36);

    makeLabel(hw, STC_ERROR, "",
              PX + 20, PY + 176, PW - 40, 22, g::fontLabel, SS_CENTER);

    makeBtn(hw, BTN_ADMIN_UNLOCK_OP, "UNLOCK",  PX + 20,  PY + 210, 186, 50);
    makeBtn(hw, BTN_CANCEL,          "CANCEL",  PX + 226, PY + 210, 186, 50);

    SetFocus(GetDlgItem(hw, EDT_TARGET_ACCT));
}

// ---- Admin — Delete Account ----
static void buildAdminDelete(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    constexpr int PW = 440, PX = (880 - PW) / 2, PY = 120;

    makeLabel(hw, STC_TITLE, "Delete Account",
              PX, PY, PW, 40, g::fontTitle, SS_CENTER);
    makeLabel(hw, 0,
              "WARNING: permanently removes the account\nand all associated data.",
              PX, PY + 48, PW, 44, g::fontSub, SS_CENTER);

    makeLabel(hw, 0, "Account Number to Delete",
              PX + 20, PY + 104, PW - 40, 22, g::fontLabel);
    makeEdit (hw, EDT_TARGET_ACCT,
              PX + 20, PY + 128, PW - 40, 36);

    makeLabel(hw, STC_ERROR, "",
              PX + 20, PY + 176, PW - 40, 22, g::fontLabel, SS_CENTER);

    makeBtn(hw, BTN_ADMIN_DELETE_OP, "DELETE",  PX + 20,  PY + 210, 186, 50);
    makeBtn(hw, BTN_CANCEL,          "CANCEL",  PX + 226, PY + 210, 186, 50);

    SetFocus(GetDlgItem(hw, EDT_TARGET_ACCT));
}

// ---- ATM Login ----
static void buildLogin(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    constexpr int PW = 380, PH = 400;
    constexpr int PX = (880 - PW) / 2, PY = (590 - PH) / 2;

    makeLabel(hw, STC_TITLE, "SecureBank",
              PX, PY, PW, 46, g::fontTitle, SS_CENTER);
    makeLabel(hw, 0, "ATM TERMINAL",
              PX, PY + 48, PW, 24, g::fontSub, SS_CENTER);

    CreateWindowA("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        PX + 20, PY + 80, PW - 40, 2,
        hw, nullptr, GetModuleHandleA(nullptr), nullptr);

    makeLabel(hw, 0, "Account Number",
              PX + 20, PY + 96, PW - 40, 22, g::fontLabel);
    makeEdit (hw, EDT_ACCOUNT,
              PX + 20, PY + 120, PW - 40, 36);

    makeLabel(hw, 0, "PIN",
              PX + 20, PY + 170, PW - 40, 22, g::fontLabel);
    makeEdit (hw, EDT_PIN,
              PX + 20, PY + 194, PW - 40, 36, true);

    makeLabel(hw, STC_ERROR, "",
              PX + 20, PY + 244, PW - 40, 22, g::fontLabel, SS_CENTER);

    makeBtn(hw, BTN_LOGIN, "LOG  IN",
            PX + 60, PY + 278, PW - 120, 52);
    makeBtn(hw, BTN_BACK, "< BACK TO PORTAL",
            PX + 80, PY + 346, PW - 160, 36);

    SetFocus(GetDlgItem(hw, EDT_ACCOUNT));
}

// ---- ATM Dashboard ----
static void buildDashboard(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    std::string welcome = "Welcome,  " + g::name;
    makeLabel(hw, STC_WELCOME, welcome.c_str(),
              20, 32, 700, 40, g::fontTitle);
    makeLabel(hw, 0,
              ("Account No:  " + std::to_string(g::acctNo)).c_str(),
              20, 78, 400, 22, g::fontLabel);

    struct BDef { int id; const char* lbl; };
    constexpr BDef btns[6] = {
        {BTN_BALANCE,  "Balance Inquiry"},
        {BTN_WITHDRAW, "Cash Withdrawal"},
        {BTN_DEPOSIT,  "Cash Deposit"},
        {BTN_TRANSFER, "Fund Transfer"},
        {BTN_MINI,     "Mini-Statement"},
        {BTN_LOGOUT,   "Logout"},
    };

    constexpr int BW = 400, BH = 72, GAP = 16, SX = 20, SY = 112;
    for (int i = 0; i < 6; ++i)
        makeBtn(hw, btns[i].id, btns[i].lbl,
                SX + (i % 2) * (BW + 36),
                SY + (i / 2) * (BH + GAP),
                BW, BH);
}

// ---- Balance ----
static void buildBalance(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "Balance Inquiry",
              20, 28, 840, 40, g::fontTitle);

    auto res = g::bank->findAccountByNumber(g::acctNo);
    if (res) {
        const auto& a = res.value();
        std::string line = std::string(a.customerName()) +
                           "   |   Account " + std::to_string(g::acctNo);
        makeLabel(hw, 0, line.c_str(), 20, 82, 840, 26, g::fontSub);
        makeLabel(hw, 0, "Available Balance",
                  20, 140, 500, 22, g::fontLabel);
        makeLabel(hw, STC_BALANCE, fmtAmt(a.balance()).c_str(),
                  20, 166, 600, 58, g::fontTitle);
    } else {
        makeLabel(hw, STC_ERROR, res.error().message.c_str(),
                  20, 120, 800, 26, g::fontSub);
    }
    makeBtn(hw, BTN_BACK, "BACK TO MENU", 20, 270, 220, 52);
}

// ---- Amount entry ----
static void buildAmount(HWND hw, const char* title, const char* hint) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, title, 20, 28, 840, 40, g::fontTitle);
    if (hint && hint[0])
        makeLabel(hw, 0, hint, 20, 78, 840, 24, g::fontSub);

    makeLabel(hw, 0, "Amount  (Rs.)", 20, 118, 500, 24, g::fontLabel);
    makeEdit (hw, EDT_AMOUNT, 20, 146, 420, 38);
    makeLabel(hw, STC_ERROR, "", 20, 196, 600, 24, g::fontLabel);
    makeBtn(hw, BTN_CONFIRM, "CONFIRM", 20,  240, 196, 52);
    makeBtn(hw, BTN_CANCEL,  "CANCEL",  232, 240, 196, 52);
    SetFocus(GetDlgItem(hw, EDT_AMOUNT));
}

// ---- Transfer ----
static void buildTransfer(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "Fund Transfer", 20, 28, 840, 40, g::fontTitle);

    makeLabel(hw, 0, "Destination Account Number",
              20, 88, 500, 24, g::fontLabel);
    makeEdit (hw, EDT_TO_ACCOUNT, 20, 116, 420, 38);

    makeLabel(hw, 0, "Amount  (Rs.)", 20, 168, 500, 24, g::fontLabel);
    makeEdit (hw, EDT_AMOUNT, 20, 196, 420, 38);
    makeLabel(hw, STC_ERROR, "", 20, 246, 600, 24, g::fontLabel);

    makeBtn(hw, BTN_CONFIRM, "TRANSFER", 20,  292, 196, 52);
    makeBtn(hw, BTN_CANCEL,  "CANCEL",   232, 292, 196, 52);
    SetFocus(GetDlgItem(hw, EDT_TO_ACCOUNT));
}

// ---- OTP Verify ----
static void buildOtp(HWND hw, double amt) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "OTP Verification Required",
              20, 28, 840, 40, g::fontTitle);

    std::string note = "Amount " + fmtAmt(amt) +
                       " exceeds Rs. 50,000 — an OTP is required.";
    makeLabel(hw, 0, note.c_str(), 20, 78, 840, 26, g::fontSub);

    std::string otpDisplay = "Your OTP (simulated SMS):    " + g::otp.generate();
    makeLabel(hw, STC_OTP_CODE, otpDisplay.c_str(),
              20, 118, 840, 32, g::fontSub);

    makeLabel(hw, 0, "Enter OTP", 20, 166, 300, 24, g::fontLabel);
    makeEdit (hw, EDT_OTP, 20, 194, 260, 38);
    makeLabel(hw, STC_ERROR, "", 20, 244, 600, 24, g::fontLabel);

    makeBtn(hw, BTN_OTP_VERIFY, "VERIFY",  20,  288, 180, 52);
    makeBtn(hw, BTN_CANCEL,     "CANCEL",  216, 288, 180, 52);
    SetFocus(GetDlgItem(hw, EDT_OTP));
}

// ---- Mini-Statement ----
static void buildMini(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "Mini-Statement  —  Last 5 Transactions",
              20, 28, 840, 40, g::fontTitle);

    makeLabel(hw, 0, "Date & Time",    20,  86, 210, 22, g::fontLabel);
    makeLabel(hw, 0, "Type",          240,  86, 160, 22, g::fontLabel);
    makeLabel(hw, 0, "Amount",        410,  86, 150, 22, g::fontLabel);
    makeLabel(hw, 0, "Balance After", 570,  86, 170, 22, g::fontLabel);

    CreateWindowA("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        20, 112, 820, 2, hw, nullptr, GetModuleHandleA(nullptr), nullptr);

    persistence::TransactionJournal jr("data/transactions.txt");
    auto allRes = jr.loadAll();
    int yRow = 120;

    if (allRes) {
        auto& all = allRes.value();
        std::vector<domain::Transaction> mine;
        for (auto& t : all)
            if (t.accountNumber() == g::acctNo)
                mine.push_back(t);

        int start = std::max(0, static_cast<int>(mine.size()) - 5);
        for (int i = start; i < static_cast<int>(mine.size()); ++i) {
            auto& t = mine[static_cast<size_t>(i)];
            const char* tp = "";
            switch (t.type()) {
                case domain::TransactionType::Deposit:     tp = "Deposit";      break;
                case domain::TransactionType::Withdrawal:  tp = "Withdrawal";   break;
                case domain::TransactionType::TransferOut: tp = "Transfer Out"; break;
                case domain::TransactionType::TransferIn:  tp = "Transfer In";  break;
            }
            std::string ts(t.timestamp());
            makeLabel(hw, 0, ts.c_str(),                    20,  yRow, 210, 22, g::fontLabel);
            makeLabel(hw, 0, tp,                            240, yRow, 160, 22, g::fontLabel);
            makeLabel(hw, 0, fmtAmt(t.amount()).c_str(),    410, yRow, 150, 22, g::fontLabel);
            makeLabel(hw, 0, fmtAmt(t.balanceAfter()).c_str(), 570, yRow, 170, 22, g::fontLabel);
            yRow += 30;
        }
        if (mine.empty())
            makeLabel(hw, 0, "No transactions on record for this account.",
                      20, yRow, 600, 22, g::fontLabel);
    } else {
        makeLabel(hw, STC_ERROR, "Could not load transaction history.",
                  20, yRow, 600, 22, g::fontLabel);
    }

    makeBtn(hw, BTN_BACK, "BACK TO MENU", 20, 480, 220, 52);
}

// ---- Success ----
static void buildSuccess(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "Operation Successful",
              20, 28, 840, 40, g::fontTitle);
    makeLabel(hw, STC_BALANCE, "\x27\x14",
              20, 78, 60, 50, g::fontTitle, SS_CENTER);

    std::istringstream ss(g::successMsg);
    std::string line;
    int y = 80;
    while (std::getline(ss, line))
        if (!line.empty()) {
            makeLabel(hw, 0, line.c_str(), 86, y, 720, 28, g::fontSub);
            y += 32;
        }

    if (!g::isAdminMode)
        makeLabel(hw, 0, "Receipt saved to the  receipts/  folder.",
                  86, y + 8, 600, 24, g::fontLabel);

    makeBtn(hw, BTN_BACK,
            g::isAdminMode ? "BACK TO ADMIN MENU" : "BACK TO MENU",
            20, y + 60, 260, 52);
}

// ---- Error ----
static void buildError(HWND hw) {
    clearAllChildren(hw);
    InvalidateRect(hw, nullptr, TRUE);

    makeLabel(hw, STC_TITLE, "Operation Failed",
              20, 28, 840, 40, g::fontTitle);
    makeLabel(hw, STC_ERROR, g::errorMsg.c_str(),
              20, 100, 820, 60, g::fontSub, SS_LEFT);
    makeBtn(hw, BTN_BACK,
            g::isAdminMode ? "BACK TO ADMIN MENU" : "BACK TO MENU",
            20, 200, 260, 52);
}

// ============================================================
//  NAVIGATION
// ============================================================
static void goTo(HWND hw, Screen s) {
    g::screen = s;
    switch (s) {
        case Screen::Portal:         buildPortal(hw);         break;
        case Screen::Login:          buildLogin(hw);          break;
        case Screen::Dashboard:      buildDashboard(hw);      break;
        case Screen::Balance:        buildBalance(hw);        break;
        case Screen::Withdraw:
            buildAmount(hw, "Cash Withdrawal",
                        "Daily withdrawal limit applies.");   break;
        case Screen::Deposit:
            buildAmount(hw, "Cash Deposit", "");             break;
        case Screen::Transfer:       buildTransfer(hw);       break;
        case Screen::OtpVerify:      buildOtp(hw, g::pendingAmt); break;
        case Screen::MiniStatement:  buildMini(hw);           break;
        case Screen::AdminLogin:     buildAdminLogin(hw);     break;
        case Screen::AdminDashboard: buildAdminDashboard(hw); break;
        case Screen::AdminCreate:    buildAdminCreate(hw);    break;
        case Screen::AdminView:      buildAdminView(hw);      break;
        case Screen::AdminSearch:    buildAdminSearch(hw);    break;
        case Screen::AdminUnlock:    buildAdminUnlock(hw);    break;
        case Screen::AdminDelete:    buildAdminDelete(hw);    break;
        case Screen::Success:        buildSuccess(hw);        break;
        case Screen::Error:          buildError(hw);          break;
    }
}

// ============================================================
//  BUSINESS-LOGIC HANDLERS — ATM
// ============================================================
static void doLogin(HWND hw) {
    std::string acctStr = getEditText(hw, EDT_ACCOUNT);
    std::string pin     = getEditText(hw, EDT_PIN);
    if (acctStr.empty() || pin.empty()) {
        setError(hw, "Please fill in both fields.");
        return;
    }
    long acct = 0;
    try   { acct = std::stol(acctStr); }
    catch (...) { setError(hw, "Invalid account number."); return; }

    auto res = g::bank->authenticate(acct, pin);
    if (!res) {
        setError(hw, res.error().message.c_str());
        SetWindowTextA(GetDlgItem(hw, EDT_PIN), "");
        SetFocus(GetDlgItem(hw, EDT_PIN));
        return;
    }
    g::acctNo = acct;
    auto aRes = g::bank->findAccountByNumber(acct);
    g::name   = aRes ? std::string(aRes.value().customerName()) : "Customer";
    goTo(hw, Screen::Dashboard);
}

static void doWithdraw(HWND hw) {
    std::string s = getEditText(hw, EDT_AMOUNT);
    double amt = 0;
    try   { amt = std::stod(s); }
    catch (...) { setError(hw, "Enter a valid amount."); return; }
    if (amt <= 0) { setError(hw, "Amount must be positive."); return; }

    g::pendingAmt  = amt;
    g::pendingType = domain::TransactionType::Withdrawal;
    if (amt > OtpService::OtpThreshold) { goTo(hw, Screen::OtpVerify); return; }

    auto res = g::bank->withdraw(g::acctNo, amt);
    if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }
    auto aRes = g::bank->findAccountByNumber(g::acctNo);
    if (aRes) ReceiptWriter::write(aRes.value(), res.value());
    g::successMsg = "Type      :  Cash Withdrawal\n"
                    "Amount    :  " + fmtAmt(amt) + "\n"
                    "Balance   :  " + fmtAmt(res.value().balanceAfter());
    goTo(hw, Screen::Success);
}

static void doDeposit(HWND hw) {
    std::string s = getEditText(hw, EDT_AMOUNT);
    double amt = 0;
    try   { amt = std::stod(s); }
    catch (...) { setError(hw, "Enter a valid amount."); return; }
    if (amt <= 0) { setError(hw, "Amount must be positive."); return; }

    auto res = g::bank->deposit(g::acctNo, amt);
    if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }
    auto aRes = g::bank->findAccountByNumber(g::acctNo);
    if (aRes) ReceiptWriter::write(aRes.value(), res.value());
    g::successMsg = "Type      :  Cash Deposit\n"
                    "Amount    :  " + fmtAmt(amt) + "\n"
                    "Balance   :  " + fmtAmt(res.value().balanceAfter());
    goTo(hw, Screen::Success);
}

static void doTransfer(HWND hw) {
    std::string toStr  = getEditText(hw, EDT_TO_ACCOUNT);
    std::string amtStr = getEditText(hw, EDT_AMOUNT);
    long   toAcct = 0;
    double amt    = 0;
    try   { toAcct = std::stol(toStr); }
    catch (...) { setError(hw, "Invalid destination account."); return; }
    try   { amt = std::stod(amtStr); }
    catch (...) { setError(hw, "Enter a valid amount."); return; }
    if (amt <= 0)            { setError(hw, "Amount must be positive."); return; }
    if (toAcct == g::acctNo) { setError(hw, "Cannot transfer to your own account."); return; }

    g::pendingAmt    = amt;
    g::pendingToAcct = toAcct;
    g::pendingType   = domain::TransactionType::TransferOut;
    if (amt > OtpService::OtpThreshold) { goTo(hw, Screen::OtpVerify); return; }

    auto res = g::bank->transfer(g::acctNo, toAcct, amt);
    if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }
    auto aRes = g::bank->findAccountByNumber(g::acctNo);
    if (aRes) ReceiptWriter::write(aRes.value(), res.value());
    g::successMsg = "Type        :  Fund Transfer\n"
                    "To Account  :  " + std::to_string(toAcct) + "\n"
                    "Amount      :  " + fmtAmt(amt) + "\n"
                    "Balance     :  " + fmtAmt(res.value().balanceAfter());
    goTo(hw, Screen::Success);
}

static void doOtpVerify(HWND hw) {
    std::string code = getEditText(hw, EDT_OTP);
    if (!g::otp.verify(code)) {
        setError(hw, "Incorrect OTP. Please try again.");
        SetWindowTextA(GetDlgItem(hw, EDT_OTP), "");
        SetFocus(GetDlgItem(hw, EDT_OTP));
        return;
    }
    if (g::pendingType == domain::TransactionType::Withdrawal) {
        auto res = g::bank->withdraw(g::acctNo, g::pendingAmt);
        if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }
        auto aRes = g::bank->findAccountByNumber(g::acctNo);
        if (aRes) ReceiptWriter::write(aRes.value(), res.value());
        g::successMsg = "Type      :  Cash Withdrawal  (OTP verified)\n"
                        "Amount    :  " + fmtAmt(g::pendingAmt) + "\n"
                        "Balance   :  " + fmtAmt(res.value().balanceAfter());
    } else {
        auto res = g::bank->transfer(g::acctNo, g::pendingToAcct, g::pendingAmt);
        if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }
        auto aRes = g::bank->findAccountByNumber(g::acctNo);
        if (aRes) ReceiptWriter::write(aRes.value(), res.value());
        g::successMsg = "Type        :  Fund Transfer  (OTP verified)\n"
                        "To Account  :  " + std::to_string(g::pendingToAcct) + "\n"
                        "Amount      :  " + fmtAmt(g::pendingAmt) + "\n"
                        "Balance     :  " + fmtAmt(res.value().balanceAfter());
    }
    goTo(hw, Screen::Success);
}

// ============================================================
//  BUSINESS-LOGIC HANDLERS — Admin
// ============================================================
static constexpr const char* ADMIN_PASSWORD = "1234";

static void doAdminLogin(HWND hw) {
    if (getEditText(hw, EDT_ADMIN_PASS) != ADMIN_PASSWORD) {
        setError(hw, "Incorrect password. Access denied.");
        SetWindowTextA(GetDlgItem(hw, EDT_ADMIN_PASS), "");
        SetFocus(GetDlgItem(hw, EDT_ADMIN_PASS));
        return;
    }
    g::isAdminMode = true;
    goTo(hw, Screen::AdminDashboard);
}

static void doAdminSearch(HWND hw) {
    std::string term = getEditText(hw, EDT_SEARCH_TERM);
    if (term.empty()) {
        setError(hw, "Enter a search term.");
        return;
    }
    setError(hw, "");

    // Case-insensitive comparison helper
    auto toLower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    std::string termLow = toLower(term);

    const auto& all = g::bank->allAccounts();
    std::vector<domain::Account> matches;
    for (const auto& a : all) {
        // Match against account number or customer name
        if (std::to_string(a.accountNumber()).find(term) != std::string::npos ||
            toLower(std::string(a.customerName())).find(termLow) != std::string::npos) {
            matches.push_back(a);
        }
    }

    HWND lb = GetDlgItem(hw, LBX_ACCOUNTS);
    if (lb) {
        populateListBox(lb, matches);
        if (matches.empty())
            setError(hw, "No accounts matched your search.");
        else {
            std::string info = std::to_string(matches.size()) + " result(s) found.";
            setError(hw, info.c_str());
        }
    }
}

static void doAdminCreate(HWND hw) {
    std::string name   = getEditText(hw, EDT_NAME);
    std::string cnic   = getEditText(hw, EDT_CNIC);
    std::string phone  = getEditText(hw, EDT_PHONE);
    std::string pin    = getEditText(hw, EDT_NEW_PIN);
    std::string balStr = getEditText(hw, EDT_OPEN_BAL);

    if (name.empty() || cnic.empty() || phone.empty() || pin.empty() || balStr.empty()) {
        setError(hw, "All fields are required.");
        return;
    }
    double bal = 0;
    try   { bal = std::stod(balStr); }
    catch (...) { setError(hw, "Opening balance must be a number."); return; }
    if (bal < 0) { setError(hw, "Opening balance cannot be negative."); return; }

    // Account numbers are auto-assigned by BankSystem (monotonically increasing)
    // — duplicates are structurally impossible.
    std::string hashed = security::PinHasher::hash(pin);
    auto res = g::bank->createAccount(name, cnic, phone,
                                      g::newAcctType, hashed, bal);
    if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }

    g::successMsg =
        "Account created successfully!\n"
        "Account No  :  " + std::to_string(res.value().accountNumber()) + "\n"
        "Name        :  " + name + "\n"
        "Type        :  " +
        (g::newAcctType == domain::AccountType::Savings ? "Savings" : "Current") + "\n"
        "Balance     :  " + fmtAmt(bal);
    goTo(hw, Screen::Success);
}

static void doAdminUnlock(HWND hw) {
    std::string s = getEditText(hw, EDT_TARGET_ACCT);
    long acct = 0;
    try   { acct = std::stol(s); }
    catch (...) { setError(hw, "Invalid account number."); return; }

    auto res = g::bank->unlockAccount(acct);
    if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }

    g::successMsg = "Account " + std::to_string(acct) + " has been unlocked.\n"
                    "The customer may now log in normally.";
    goTo(hw, Screen::Success);
}

static void doAdminDelete(HWND hw) {
    std::string s = getEditText(hw, EDT_TARGET_ACCT);
    long acct = 0;
    try   { acct = std::stol(s); }
    catch (...) { setError(hw, "Invalid account number."); return; }

    auto res = g::bank->deleteAccount(acct);
    if (!res) { g::errorMsg = res.error().message; goTo(hw, Screen::Error); return; }

    g::successMsg = "Account " + std::to_string(acct) +
                    " has been permanently deleted.";
    goTo(hw, Screen::Success);
}

// ============================================================
//  WINDOW PROCEDURE
// ============================================================
static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_PAINT:
        paintBackground(hw);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DRAWITEM:
        paintButton(reinterpret_cast<LPDRAWITEMSTRUCT>(lp));
        return TRUE;

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkColor  (dc, clr::inputBg);
        SetTextColor(dc, clr::white);
        return reinterpret_cast<LRESULT>(g::brInput);
    }

    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkColor  (dc, clr::listBg);
        SetTextColor(dc, clr::muted);
        return reinterpret_cast<LRESULT>(g::brList);
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp);
        int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
        SetBkMode(dc, TRANSPARENT);
        switch (id) {
            case STC_TITLE:      SetTextColor(dc, clr::brand);   break;
            case STC_ERROR:      SetTextColor(dc, clr::danger);  break;
            case STC_BALANCE:    SetTextColor(dc, clr::success); break;
            case STC_OTP_CODE:   SetTextColor(dc, clr::brand);   break;
            case STC_WELCOME:    SetTextColor(dc, clr::white);   break;
            case STC_LIST_HDR:   SetTextColor(dc, clr::brand);   break;
            default:             SetTextColor(dc, clr::muted);   break;
        }
        return reinterpret_cast<LRESULT>(g::brBg);
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
            // Portal
            case BTN_USER_MODE:
                g::isAdminMode = false;
                goTo(hw, Screen::Login);
                break;
            case BTN_ADMIN_MODE:
                goTo(hw, Screen::AdminLogin);
                break;

            // ATM login
            case BTN_LOGIN: doLogin(hw); break;

            // ATM dashboard
            case BTN_BALANCE:    goTo(hw, Screen::Balance);       break;
            case BTN_WITHDRAW:   goTo(hw, Screen::Withdraw);      break;
            case BTN_DEPOSIT:    goTo(hw, Screen::Deposit);       break;
            case BTN_TRANSFER:   goTo(hw, Screen::Transfer);      break;
            case BTN_MINI:       goTo(hw, Screen::MiniStatement); break;
            case BTN_LOGOUT:
                g::acctNo = 0; g::name.clear();
                goTo(hw, Screen::Portal);
                break;

            // ATM operations
            case BTN_CONFIRM:
                if      (g::screen == Screen::Withdraw) doWithdraw(hw);
                else if (g::screen == Screen::Deposit)  doDeposit(hw);
                else if (g::screen == Screen::Transfer) doTransfer(hw);
                break;
            case BTN_OTP_VERIFY: doOtpVerify(hw); break;

            // Admin login
            case BTN_ADMIN_LOGIN: doAdminLogin(hw); break;

            // Admin dashboard navigation
            case BTN_TO_CREATE:  goTo(hw, Screen::AdminCreate); break;
            case BTN_TO_VIEW:    goTo(hw, Screen::AdminView);   break;
            case BTN_TO_SEARCH:  goTo(hw, Screen::AdminSearch); break;
            case BTN_TO_UNLOCK:  goTo(hw, Screen::AdminUnlock); break;
            case BTN_TO_DELETE:  goTo(hw, Screen::AdminDelete); break;
            case BTN_ADMIN_LOGOUT:
                g::isAdminMode = false;
                goTo(hw, Screen::Portal);
                break;

            // Admin operations
            case BTN_ADMIN_CREATE_OP: doAdminCreate(hw); break;
            case BTN_ADMIN_SEARCH_OP: doAdminSearch(hw); break;
            case BTN_ADMIN_UNLOCK_OP: doAdminUnlock(hw); break;
            case BTN_ADMIN_DELETE_OP: doAdminDelete(hw); break;

            // Account type toggle
            case BTN_TYPE_SAVINGS:
                g::newAcctType = domain::AccountType::Savings;
                InvalidateRect(GetDlgItem(hw, BTN_TYPE_SAVINGS), nullptr, TRUE);
                InvalidateRect(GetDlgItem(hw, BTN_TYPE_CURRENT), nullptr, TRUE);
                break;
            case BTN_TYPE_CURRENT:
                g::newAcctType = domain::AccountType::Current;
                InvalidateRect(GetDlgItem(hw, BTN_TYPE_SAVINGS), nullptr, TRUE);
                InvalidateRect(GetDlgItem(hw, BTN_TYPE_CURRENT), nullptr, TRUE);
                break;

            // Shared cancel / back
            case BTN_CANCEL:
                if (g::isAdminMode) goTo(hw, Screen::AdminDashboard);
                else                goTo(hw, Screen::Dashboard);
                break;
            case BTN_BACK:
                if (g::screen == Screen::Login ||
                    g::screen == Screen::AdminLogin)
                    goTo(hw, Screen::Portal);
                else if (g::isAdminMode)
                    goTo(hw, Screen::AdminDashboard);
                else
                    goTo(hw, Screen::Dashboard);
                break;

            // Search box: also trigger on Enter key inside edit
            default:
                if (HIWORD(wp) == EN_CHANGE &&
                    id == EDT_SEARCH_TERM &&
                    g::screen == Screen::AdminSearch) {
                    // live search — optional, harmless if blank
                }
                break;
        }
        return 0;
    }

    // Allow pressing Enter in the search box to trigger search
    case WM_KEYDOWN:
        if (wp == VK_RETURN && g::screen == Screen::AdminSearch)
            doAdminSearch(hw);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}

// ============================================================
//  PUBLIC ENTRY POINT
// ============================================================
int runGui(core::BankSystem& bank) {
    g::bank = &bank;

    HINSTANCE inst = GetModuleHandleA(nullptr);

    auto makeFont = [](int size, int weight, const char* face) -> HFONT {
        return CreateFontA(size, 0, 0, 0, weight,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    };
    g::fontTitle = makeFont(-32, FW_BOLD,   "Segoe UI");
    g::fontSub   = makeFont(-17, FW_NORMAL, "Segoe UI");
    g::fontLabel = makeFont(-15, FW_NORMAL, "Segoe UI");
    g::fontInput = makeFont(-15, FW_NORMAL, "Segoe UI");
    g::fontBtn   = makeFont(-15, FW_BOLD,   "Segoe UI");
    g::fontMono  = makeFont(-14, FW_NORMAL, "Consolas");

    g::brBg    = CreateSolidBrush(clr::bg);
    g::brInput = CreateSolidBrush(clr::inputBg);
    g::brList  = CreateSolidBrush(clr::listBg);

    WNDCLASSEXA wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = inst;
    wc.hbrBackground = g::brBg;
    wc.lpszClassName = "SecureBankATM";
    wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconA(nullptr, IDI_APPLICATION);
    RegisterClassExA(&wc);

    constexpr int WW = 880, WH = 600;
    const int sx = (GetSystemMetrics(SM_CXSCREEN) - WW) / 2;
    const int sy = (GetSystemMetrics(SM_CYSCREEN) - WH) / 2;

    HWND hw = CreateWindowExA(
        0, "SecureBankATM", "SecureBank  \xe2\x80\x94  Banking System",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        sx, sy, WW, WH,
        nullptr, nullptr, inst, nullptr);

    if (!hw) return 1;

    goTo(hw, Screen::Portal);
    ShowWindow(hw, SW_SHOW);
    UpdateWindow(hw);

    MSG m = {};
    while (GetMessageA(&m, nullptr, 0, 0)) {
        if (!IsDialogMessageA(hw, &m)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }

    DeleteObject(g::fontTitle);
    DeleteObject(g::fontSub);
    DeleteObject(g::fontLabel);
    DeleteObject(g::fontInput);
    DeleteObject(g::fontBtn);
    DeleteObject(g::fontMono);
    DeleteObject(g::brBg);
    DeleteObject(g::brInput);
    DeleteObject(g::brList);

    return static_cast<int>(m.wParam);
}

} // namespace securebank::ui

#endif // _WIN32
