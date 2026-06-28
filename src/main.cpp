// ============================================================
//  REvenge Installer  —  main.cpp
//  Win32 API + C++17  |  No external deps
//  Windows 10/11, 4-thread DL, self-delete
// ============================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <wininet.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <versionhelpers.h>

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

#include "logo_data.h"

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' "\
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "\
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

constexpr wchar_t DL_URL[] = L"https://github.com/RevengeUpdater/REvenge-Releases/releases/download/v1.5.0/REvenge_Setup_1.5.0.exe";
constexpr wchar_t DL_NAME[] = L"REvenge_Setup_1.5.0.exe";
constexpr wchar_t APP_VERSION[] = L"v1.5.0";
constexpr int NUM_THREADS = 4;

constexpr UINT WM_DL_PROGRESS = WM_USER + 1;
constexpr UINT WM_DL_DONE = WM_USER + 2;

// ── Palette (BGR) ────────────────────────────────────────────
constexpr COLORREF CLR_BG       = 0x00121212;
constexpr COLORREF CLR_SIDEBAR  = 0x001A1A1A;
constexpr COLORREF CLR_CARD     = 0x001E1E1E;
constexpr COLORREF CLR_CARD_IN  = 0x00262626;
constexpr COLORREF CLR_BORDER   = 0x00333333;
constexpr COLORREF CLR_TEXT     = 0x00FFFFFF;
constexpr COLORREF CLR_SUB      = 0x00AAAAAA;
constexpr COLORREF CLR_MUTED    = 0x00666666;
constexpr COLORREF CLR_CYAN     = 0x00FFF200;   // #00F2FF
constexpr COLORREF CLR_PURPLE   = 0x00FF00AD;   // #AD00FF
constexpr COLORREF CLR_PB_BG    = 0x002A2A2A;

// ── DPI ──────────────────────────────────────────────────────
static int g_dpi = 96;
static inline int S(int v) { return MulDiv(v, g_dpi, 96); }

// ── Локализация ──────────────────────────────────────────────
struct Lang {
    const wchar_t* langName;
    const wchar_t* langSection;
    const wchar_t* statusReady;
    const wchar_t* statusConn;
    const wchar_t* statusDone;
    const wchar_t* statusFail;
    const wchar_t* githubPending;
    const wchar_t* btnDownload;
    const wchar_t* btnLoading;
    const wchar_t* btnLaunch;
    const wchar_t* btnRetry;
    const wchar_t* btnSettings;
    const wchar_t* footerHwPassed;
    const wchar_t* footerHwFailed;
    const wchar_t* footerConnOptimal;
    const wchar_t* footerConnOffline;
    const wchar_t* footerThreadsFmt;
    const wchar_t* settingsTitle;
    const wchar_t* settingsBody;
};

static constexpr Lang kLangEN = {
    L"English", L"Select Your Language",
    L"Ready to download", L"Connecting to GitHub...",
    L"Download complete \u2014 launching installer...",
    L"Download failed. Click to retry.",
    L"GitHub connection pending",
    L"Download and Install", L"Downloading...", L"Launching installer...",
    L"Download Failed \u2014 Retry", L"Settings",
    L"Hardware Check: Passed.", L"Hardware Check: Failed.",
    L"Connection: Optimal.", L"Connection: Offline.",
    L"%d active threads", L"REvenge Installer",
    L"REvenge Installer v1.5.0\n4 parallel download threads\ngithub.com/RevengeUpdater",
};

static constexpr Lang kLangRU = {
    L"\u0420\u0443\u0441\u0441\u043a\u0438\u0439", L"\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u044f\u0437\u044b\u043a",
    L"\u0413\u043e\u0442\u043e\u0432 \u043a \u0437\u0430\u0433\u0440\u0443\u0437\u043a\u0435",
    L"\u041f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435 \u043a GitHub...",
    L"\u0417\u0430\u0433\u0440\u0443\u0437\u043a\u0430 \u0437\u0430\u0432\u0435\u0440\u0448\u0435\u043d\u0430 \u2014 \u0437\u0430\u043f\u0443\u0441\u043a...",
    L"\u041e\u0448\u0438\u0431\u043a\u0430. \u041d\u0430\u0436\u043c\u0438\u0442\u0435 \u0434\u043b\u044f \u043f\u043e\u0432\u0442\u043e\u0440\u0430.",
    L"\u041e\u0436\u0438\u0434\u0430\u043d\u0438\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u044f \u043a GitHub",
    L"\u0421\u043a\u0430\u0447\u0430\u0442\u044c \u0438 \u0423\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u044c",
    L"\u0417\u0430\u0433\u0440\u0443\u0437\u043a\u0430...", L"\u0417\u0430\u043f\u0443\u0441\u043a \u0443\u0441\u0442\u0430\u043d\u043e\u0432\u0449\u0438\u043a\u0430...",
    L"\u041e\u0448\u0438\u0431\u043a\u0430 \u2014 \u041f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u044c", L"\u041d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438",
    L"\u041f\u0440\u043e\u0432\u0435\u0440\u043a\u0430 \u0416\u0415: \u041f\u0440\u043e\u0439\u0434\u0435\u043d\u0430.",
    L"\u041f\u0440\u043e\u0432\u0435\u0440\u043a\u0430 \u0416\u0415: \u041d\u0435 \u043f\u0440\u043e\u0439\u0434\u0435\u043d\u0430.",
    L"\u0421\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u0435: \u041e\u043f\u0442\u0438\u043c\u0430\u043b\u044c\u043d\u043e\u0435.",
    L"\u0421\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u0435: \u041d\u0435\u0442 \u0441\u0435\u0442\u0438.",
    L"%d \u0430\u043a\u0442\u0438\u0432\u043d\u044b\u0445 \u043f\u043e\u0442\u043e\u043a\u0430",
    L"REvenge \u0423\u0441\u0442\u0430\u043d\u043e\u0432\u0449\u0438\u043a",
    L"REvenge \u0423\u0441\u0442\u0430\u043d\u043e\u0432\u0449\u0438\u043a v1.5.0\n4 \u043f\u043e\u0442\u043e\u043a\u0430 \u0437\u0430\u0433\u0440\u0443\u0437\u043a\u0438",
};

static constexpr Lang kLangTR = {
    L"T\u00fcrk\u00e7e", L"Dilinizi Se\u00e7in",
    L"\u0130ndirmeye haz\u0131r", L"GitHub'a ba\u011flan\u0131l\u0131yor...",
    L"\u0130ndirme tamamland\u0131 \u2014 y\u00fckleyici ba\u015flat\u0131l\u0131yor...",
    L"\u0130ndirme ba\u015far\u0131s\u0131z. Tekrar denemek i\u00e7in t\u0131klay\u0131n.",
    L"GitHub ba\u011flant\u0131s\u0131 bekleniyor",
    L"\u0130ndir ve Kur", L"\u0130ndiriliyor...", L"Y\u00fckleyici ba\u015flat\u0131l\u0131yor...",
    L"Ba\u015far\u0131s\u0131z \u2014 Tekrar Dene", L"Ayarlar",
    L"Donan\u0131m Kontrol\u00fc: Ge\u00e7ti.", L"Donan\u0131m Kontrol\u00fc: Ba\u015far\u0131s\u0131z.",
    L"Ba\u011flant\u0131: Optimal.", L"Ba\u011flant\u0131: \u00c7evrimd\u0131\u015f\u0131.",
    L"%d aktif i\u015f par\u00e7ac\u0131\u011f\u0131", L"REvenge Y\u00fckleyici",
    L"REvenge Y\u00fckleyici v1.5.0\n4 paralel indirme i\u015f par\u00e7ac\u0131\u011f\u0131",
};

static constexpr Lang kLangPT = {
    L"Portugu\u00eas", L"Selecione seu Idioma",
    L"Pronto para baixar", L"Conectando ao GitHub...",
    L"Download conclu\u00eddo \u2014 iniciando instalador...",
    L"Falha no download. Clique para tentar novamente.",
    L"Conex\u00e3o GitHub pendente",
    L"Baixar e Instalar", L"Baixando...", L"Iniciando instalador...",
    L"Falha \u2014 Tentar Novamente", L"Configura\u00e7\u00f5es",
    L"Verifica\u00e7\u00e3o de Hardware: Aprovada.", L"Verifica\u00e7\u00e3o de Hardware: Falhou.",
    L"Conex\u00e3o: \u00d3tima.", L"Conex\u00e3o: Offline.",
    L"%d threads ativas", L"REvenge Instalador",
    L"REvenge Instalador v1.5.0\n4 threads de download paralelo",
};

static const Lang* g_langs[] = { &kLangEN, &kLangRU, &kLangTR, &kLangPT };
static constexpr int NUM_LANGS = (int)(sizeof(g_langs) / sizeof(g_langs[0]));
static const Lang* g_lang = &kLangRU;
static int g_selLang = 1;

// ── Состояние загрузки ───────────────────────────────────────
struct DownloadThread {
    LONGLONG downloaded, speed, start, end;
    bool done, ok;
    DownloadThread() : downloaded(0), speed(0), start(0), end(0), done(false), ok(false) {}
};

static DownloadThread        g_ts[NUM_THREADS];
static std::atomic<LONGLONG> g_totalSize(0);
static std::atomic<int>      g_doneCnt(0);
static std::atomic<bool>     g_downloading(false);
static std::atomic<bool>     g_allDone(false);
static std::atomic<bool>     g_success(false);
static std::wstring          g_savePath;
static std::wstring          g_status;
static std::mutex            g_stMtx;
static bool                  g_online = true;
static bool                  g_hwOk = true;
static std::wstring          g_osString;

static void SetSt(const std::wstring& s) {
    std::lock_guard<std::mutex> lk(g_stMtx); g_status = s;
}
static std::wstring GetSt() {
    std::lock_guard<std::mutex> lk(g_stMtx); return g_status;
}

// ── Глобальные хэндлы ────────────────────────────────────────
static HWND    g_hWnd = nullptr;
static HBITMAP g_hLogo = nullptr;
static int     g_logoSz = 0;

static int  g_hovLang = -1, g_pressLang = -1;
static bool g_hovDl = false, g_pressDl = false;
static bool g_hovSet = false, g_pressSet = false;

static HFONT g_fBrand, g_fBrandSm, g_fTitle, g_fNorm, g_fSmall, g_fBtn, g_fFooter;

static void MakeFonts()
{
    auto MF = [](int pt, bool b, const wchar_t* f = L"Segoe UI Variable")->HFONT {
        return CreateFont(S(-pt), 0, 0, 0, b ? FW_BOLD : FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, f);
    };
    g_fBrand   = MF(26, true);
    g_fBrandSm = MF(26, true);
    g_fTitle   = MF(13, false);
    g_fNorm    = MF(12, false);
    g_fSmall   = MF(10, false);
    g_fBtn     = MF(13, true);
    g_fFooter  = MF(9, false);
}

static void FreeFonts()
{
    DeleteObject(g_fBrand);   DeleteObject(g_fBrandSm);
    DeleteObject(g_fTitle);  DeleteObject(g_fNorm);
    DeleteObject(g_fSmall);  DeleteObject(g_fBtn);
    DeleteObject(g_fFooter);
}

// ── Acrylic / blur backdrop (Win11 + Win10 fallback) ───────────
enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};
struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};
enum WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19
};
struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attribute;
    PVOID Data;
    SIZE_T sizeOfData;
};
using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

static void EnableAcrylicBackdrop(HWND hwnd)
{
    if (IsWindows10OrGreater()) {
        DWORD backdrop = DWMSBT_TRANSIENTWINDOW;
        if (SUCCEEDED(DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
            &backdrop, sizeof(backdrop))))
        {
            MARGINS mg = { 0, 0, 0, 0 };
            DwmExtendFrameIntoClientArea(hwnd, &mg);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            return;
        }
    }

    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    auto pSetWCA = hUser
        ? (SetWindowCompositionAttributeFn)GetProcAddress(hUser, "SetWindowCompositionAttribute")
        : nullptr;
    if (pSetWCA) {
        ACCENT_POLICY policy{};
        policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
        policy.AccentFlags = 2;
        policy.GradientColor = 0x66121212;
        WINDOWCOMPOSITIONATTRIBDATA data{};
        data.Attribute = WCA_ACCENT_POLICY;
        data.Data = &policy;
        data.sizeOfData = sizeof(policy);
        pSetWCA(hwnd, &data);
    }
    else {
        MARGINS mg = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &mg);
    }
}

// ── Утилиты рисования ────────────────────────────────────────
static void FillR(HDC dc, RECT r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b);
}

static void FillRR(HDC dc, RECT r, int rad, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c); HPEN p = CreatePen(PS_NULL, 0, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}

static void FillRRAlpha(HDC dc, RECT r, int rad, COLORREF c, BYTE alpha)
{
    if (alpha == 0) return;
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;
    if (alpha == 255) { FillRR(dc, r, rad, c); return; }

    HDC tmp = CreateCompatibleDC(dc);
    HBITMAP hb = CreateCompatibleBitmap(dc, w, h);
    HGDIOBJ old = SelectObject(tmp, hb);
    RECT lr = { 0, 0, w, h };
    FillRR(tmp, lr, rad, c);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, alpha, 0 };
    AlphaBlend(dc, r.left, r.top, w, h, tmp, 0, 0, w, h, bf);
    SelectObject(tmp, old);
    DeleteObject(hb);
    DeleteDC(tmp);
}

static void FillRAlpha(HDC dc, RECT r, COLORREF c, BYTE alpha)
{
    FillRRAlpha(dc, r, 0, c, alpha);
}

static void StrokeRR(HDC dc, RECT r, int rad, COLORREF c, int w = 1)
{
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    HPEN p = CreatePen(PS_SOLID, w, c); HPEN op = (HPEN)SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, op); DeleteObject(p);
}

static void TL(HDC dc, const wchar_t* s, RECT r, COLORREF c, HFONT f,
    UINT fl = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS)
{
    SelectObject(dc, f); SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c); DrawText(dc, s, -1, &r, fl);
}

static void TC(HDC dc, const wchar_t* s, RECT r, COLORREF c, HFONT f)
{
    TL(dc, s, r, c, f, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void FillHGradient(HDC dc, RECT r, COLORREF c1, COLORREF c2)
{
    TRIVERTEX tv[2] = {
        { r.left, r.top, (COLOR16)(GetRValue(c1) << 8), (COLOR16)(GetGValue(c1) << 8), (COLOR16)(GetBValue(c1) << 8), 0 },
        { r.right, r.bottom, (COLOR16)(GetRValue(c2) << 8), (COLOR16)(GetGValue(c2) << 8), (COLOR16)(GetBValue(c2) << 8), 0 },
    };
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(dc, tv, 2, &gr, 1, GRADIENT_FILL_RECT_H);
}

static void FillRRGradient(HDC dc, RECT r, int rad, COLORREF c1, COLORREF c2)
{
    HRGN rg = CreateRoundRectRgn(r.left, r.top, r.right, r.bottom, rad, rad);
    SelectClipRgn(dc, rg);
    FillHGradient(dc, r, c1, c2);
    SelectClipRgn(dc, nullptr);
    DeleteObject(rg);
}

static void DrawGlowBorder(HDC dc, RECT rc, int pad)
{
    for (int i = pad; i >= 1; --i) {
        float t = (float)i / pad;
        int rr = (int)(GetRValue(CLR_CYAN) * t + GetRValue(CLR_PURPLE) * (1.f - t));
        int gg = (int)(GetGValue(CLR_CYAN) * t + GetGValue(CLR_PURPLE) * (1.f - t));
        int bb = (int)(GetBValue(CLR_CYAN) * t + GetBValue(CLR_PURPLE) * (1.f - t));
        COLORREF c = RGB(rr, gg, bb);
        RECT fr = { rc.left + i - 1, rc.top + i - 1, rc.right - i + 1, rc.bottom - i + 1 };
        BYTE a = (BYTE)(80 + (255 - 80) * (pad - i + 1) / pad);
        FillRRAlpha(dc, fr, S(12), c, a);
        StrokeRR(dc, fr, S(12), c, 1);
    }
}

static HBITMAP MakeLogoBmp(int sz)
{
    HDC hdc = GetDC(nullptr);
    HDC src = CreateCompatibleDC(hdc);
    HDC dst = CreateCompatibleDC(hdc);

    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = LOGO_W; bi.bmiHeader.biHeight = -LOGO_H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* p = nullptr;
    HBITMAP hS = CreateDIBSection(src, &bi, DIB_RGB_COLORS, &p, nullptr, 0);
    if (!hS) {
        DeleteDC(src); DeleteDC(dst); ReleaseDC(nullptr, hdc);
        return nullptr;
    }
    if (p) memcpy(p, LOGO_BGRA, LOGO_W * LOGO_H * 4);

    bi.bmiHeader.biWidth = sz; bi.bmiHeader.biHeight = -sz;
    void* pd = nullptr;
    HBITMAP hD = CreateDIBSection(dst, &bi, DIB_RGB_COLORS, &pd, nullptr, 0);
    if (!hD) {
        DeleteObject(hS);
        DeleteDC(src); DeleteDC(dst); ReleaseDC(nullptr, hdc);
        return nullptr;
    }

    HBITMAP oS = (HBITMAP)SelectObject(src, hS);
    HBITMAP oD = (HBITMAP)SelectObject(dst, hD);
    SetStretchBltMode(dst, HALFTONE);
    StretchBlt(dst, 0, 0, sz, sz, src, 0, 0, LOGO_W, LOGO_H, SRCCOPY);
    SelectObject(src, oS); SelectObject(dst, oD);
    DeleteDC(src); DeleteDC(dst); DeleteObject(hS);
    ReleaseDC(nullptr, hdc);
    return hD;
}

static void DrawLogo(HDC dc, int x, int y, int sz)
{
    if (!g_hLogo) return;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP old = (HBITMAP)SelectObject(mem, g_hLogo);
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dc, x, y, sz, sz, mem, 0, 0, sz, sz, bf);
    SelectObject(mem, old); DeleteDC(mem);
}

static void DrawThreadsIcon(HDC dc, int x, int y, int h, COLORREF c)
{
    HPEN p = CreatePen(PS_SOLID, S(2), c);
    HPEN op = (HPEN)SelectObject(dc, p);
    int barW = S(3), gap = S(4);
    for (int i = 0; i < 3; ++i) {
        int bh = h - i * S(4);
        int bx = x + i * (barW + gap);
        MoveToEx(dc, bx, y + h, nullptr);
        LineTo(dc, bx, y + h - bh);
        LineTo(dc, bx + barW, y + h - bh);
        LineTo(dc, bx + barW, y + h);
    }
    SelectObject(dc, op); DeleteObject(p);
}

// ── Системная информация ─────────────────────────────────────
static std::wstring QueryOsString()
{
    std::wstring result = L"Windows";
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        wchar_t product[128] = {}, display[64] = {}, build[32] = {};
        DWORD sz = sizeof(product);
        RegQueryValueExW(hKey, L"ProductName", nullptr, nullptr, (LPBYTE)product, &sz);
        sz = sizeof(display);
        if (RegQueryValueExW(hKey, L"DisplayVersion", nullptr, nullptr, (LPBYTE)display, &sz) != ERROR_SUCCESS)
            RegQueryValueExW(hKey, L"ReleaseId", nullptr, nullptr, (LPBYTE)display, &sz);
        sz = sizeof(build);
        RegQueryValueExW(hKey, L"CurrentBuild", nullptr, nullptr, (LPBYTE)build, &sz);
        RegCloseKey(hKey);
        result = product;
        if (display[0]) { result += L" "; result += display; }
        else if (build[0]) { result += L" Build "; result += build; }
    }
    return result;
}

static bool CheckHardware()
{
    MEMORYSTATUSEX ms{ sizeof(ms) };
    if (!GlobalMemoryStatusEx(&ms)) return true;
    return ms.ullTotalPhys >= (2ULL << 30);
}

static bool CheckOnline()
{
    DWORD flags = 0;
    return InternetGetConnectedState(&flags, 0) != FALSE;
}

// ── Загрузка ─────────────────────────────────────────────────
static void WatchAndDeleteFile(const std::wstring& filePath)
{
    Sleep(2000);
    DWORD targetPid = 0;
    for (int attempt = 0; attempt < 60 && targetPid == 0; attempt++) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
            if (Process32FirstW(snap, &pe)) {
                do {
                    if (_wcsicmp(pe.szExeFile, L"REvenge_Setup_1.5.0.exe") == 0) {
                        targetPid = pe.th32ProcessID; break;
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }
        if (!targetPid) Sleep(1000);
    }
    if (targetPid) {
        HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, targetPid);
        if (hProc) { WaitForSingleObject(hProc, INFINITE); CloseHandle(hProc); }
    }
    else Sleep(60000);
    Sleep(2000);
    DeleteFileW(filePath.c_str());
}

static void DlRange(int idx, HWND hWnd)
{
    DownloadThread& ts = g_ts[idx];
    HINTERNET hI = InternetOpenW(L"RVG/1.5", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hI) {
        ts.done = true; ts.ok = false;
        if (++g_doneCnt == NUM_THREADS) PostMessage(hWnd, WM_DL_DONE, 0, 0); return;
    }

    wchar_t hdr[128];
    swprintf_s(hdr, 128, L"Range: bytes=%lld-%lld\r\n", ts.start, ts.end);
    HINTERNET hU = InternetOpenUrlW(hI, DL_URL, hdr, (DWORD)wcslen(hdr),
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
        INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID, 0);
    if (!hU) {
        InternetCloseHandle(hI);
        ts.done = true; ts.ok = false;
        if (++g_doneCnt == NUM_THREADS) PostMessage(hWnd, WM_DL_DONE, 0, 0); return;
    }

    HANDLE hF = CreateFileW(g_savePath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hF == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hU); InternetCloseHandle(hI);
        ts.done = true; ts.ok = false;
        if (++g_doneCnt == NUM_THREADS) PostMessage(hWnd, WM_DL_DONE, 0, 0); return;
    }

    LARGE_INTEGER li; li.QuadPart = ts.start;
    SetFilePointerEx(hF, li, nullptr, FILE_BEGIN);

    constexpr DWORD CHUNK = 131072;
    auto buf = std::make_unique<BYTE[]>(CHUNK);
    ULONGLONG lastTick = GetTickCount64(); LONGLONG lastB = 0;

    while (true) {
        DWORD r = 0;
        if (!InternetReadFile(hU, buf.get(), CHUNK, &r) || r == 0) break;
        DWORD w = 0; WriteFile(hF, buf.get(), r, &w, nullptr);
        ts.downloaded += r;
        ULONGLONG now = GetTickCount64();
        ULONGLONG el = now - lastTick;
        if (el >= 400) {
            ts.speed = (ts.downloaded - lastB) * 1000 / (LONGLONG)el;
            lastTick = now; lastB = ts.downloaded;
        }
        PostMessage(hWnd, WM_DL_PROGRESS, 0, 0);
    }
    CloseHandle(hF);
    InternetCloseHandle(hU); InternetCloseHandle(hI);
    ts.ok = true; ts.done = true;
    if (++g_doneCnt == NUM_THREADS) PostMessage(hWnd, WM_DL_DONE, 1, 0);
}

static void DlMaster(HWND hWnd)
{
    wchar_t dl[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, dl)))
        GetTempPathW(MAX_PATH, dl);
    wchar_t tmp[MAX_PATH]; wcscpy_s(tmp, MAX_PATH, dl); wcscat_s(tmp, MAX_PATH, L"\\..\\Downloads");
    wchar_t res[MAX_PATH]; GetFullPathNameW(tmp, MAX_PATH, res, nullptr);
    if (GetFileAttributesW(res) != INVALID_FILE_ATTRIBUTES) wcscpy_s(dl, MAX_PATH, res);
    g_savePath = std::wstring(dl) + L"\\" + DL_NAME;

    HINTERNET hI = InternetOpenW(L"RVG/1.5", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    HINTERNET hU = InternetOpenUrlW(hI, DL_URL, nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID, 0);
    LONGLONG fSz = 0;
    if (hU) {
        wchar_t lb[32] = {}; DWORD ls = sizeof(lb);
        if (HttpQueryInfoW(hU, HTTP_QUERY_CONTENT_LENGTH, lb, &ls, nullptr))
            fSz = _wtoll(lb);
        InternetCloseHandle(hU);
    }
    if (hI) InternetCloseHandle(hI);
    g_totalSize = fSz;

    HANDLE hF = CreateFileW(g_savePath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hF == INVALID_HANDLE_VALUE) { PostMessage(hWnd, WM_DL_DONE, 0, 0); return; }
    if (fSz > 0) {
        LARGE_INTEGER li; li.QuadPart = fSz - 1;
        SetFilePointerEx(hF, li, nullptr, FILE_BEGIN);
        BYTE z = 0; DWORD w; WriteFile(hF, &z, 1, &w, nullptr);
    }
    CloseHandle(hF);

    LONGLONG chunk = fSz > 0 ? fSz / NUM_THREADS : 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        g_ts[i].downloaded = 0; g_ts[i].speed = 0;
        g_ts[i].done = false; g_ts[i].ok = false;
        g_ts[i].start = i * chunk;
        g_ts[i].end = (i == NUM_THREADS - 1) ? (fSz > 0 ? fSz - 1 : 0) : (i + 1) * chunk - 1;
        std::thread(DlRange, i, hWnd).detach();
    }
}

// ── Геометрия ────────────────────────────────────────────────
static const int SIDEBAR_W = 200;
static const int FOOTER_H  = 34;
static const int PAD       = 16;

static int SidebarW() { return S(SIDEBAR_W); }
static int FooterH()  { return S(FOOTER_H); }

static RECT LangBtnRect(HWND hw, int idx)
{
    RECT rc; GetClientRect(hw, &rc);
    int sw = SidebarW(), fh = FooterH();
    int x0 = sw + S(PAD), y0 = S(PAD);
    int cw = rc.right - x0 - S(PAD);
    int langCardH = S(108);
    int btnAreaTop = y0 + S(44);
    int btnH = S(32);
    int gap = S(8);
    int totalW = cw - S(28);
    int btnW = (totalW - gap * (NUM_LANGS - 1)) / NUM_LANGS;
    int x = x0 + S(14) + idx * (btnW + gap);
    return { x, btnAreaTop, x + btnW, btnAreaTop + btnH };
}

static RECT DownloadBtnRect(HWND hw)
{
    RECT rc; GetClientRect(hw, &rc);
    int sw = SidebarW(), fh = FooterH();
    int x0 = sw + S(PAD), cw = rc.right - x0 - S(PAD);
    int dlCardTop = S(PAD) + S(108) + S(PAD);
    int dlCardH = rc.bottom - fh - dlCardTop - S(PAD);
    int btnH = S(42);
    int settingsH = S(36);
    int gap = S(10);
    int y = dlCardTop + dlCardH - S(14) - settingsH - gap - btnH;
    return { x0 + S(14), y, x0 + cw - S(14), y + btnH };
}

static RECT SettingsBtnRect(HWND hw)
{
    RECT dr = DownloadBtnRect(hw);
    int btnH = S(36);
    int y = dr.bottom + S(10);
    return { dr.left, y, dr.right, y + btnH };
}

// ── Отрисовка ────────────────────────────────────────────────
static void Paint(HWND hw)
{
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hw, &ps);
    RECT rc; GetClientRect(hw, &rc);
    int W = rc.right, H = rc.bottom;

    // ── Double buffering: рисуем в offscreen bitmap, потом одним BitBlt на экран ──
    HDC     memDC  = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, W, H);
    HGDIOBJ memOld = SelectObject(memDC, memBmp);
    HDC     hdc_orig = hdc;
    hdc = memDC; // все операции ниже идут в offscreen

    FillRAlpha(hdc, rc, CLR_BG, 28);
    DrawGlowBorder(hdc, rc, S(3));

    int sw = SidebarW(), fh = FooterH();

    FillRAlpha(hdc, { 0, 0, sw, H - fh }, CLR_SIDEBAR, 175);

    int logoSz = S(56);
    int logoX = (sw - logoSz) / 2;
    int logoY = S(48);
    DrawLogo(hdc, logoX, logoY, logoSz);

    {
        RECT r1 = { S(12), logoY + logoSz + S(20), sw - S(12), logoY + logoSz + S(52) };
        TC(hdc, L"REvenge", r1, CLR_TEXT, g_fBrand);
        RECT r2 = { S(12), logoY + logoSz + S(50), sw - S(12), logoY + logoSz + S(82) };
        TC(hdc, L"Installer", r2, CLR_TEXT, g_fBrandSm);
        RECT r3 = { S(12), logoY + logoSz + S(82), sw - S(12), logoY + logoSz + S(102) };
        TC(hdc, APP_VERSION, r3, CLR_SUB, g_fSmall);
    }

    int x0 = sw + S(PAD), cw = W - x0 - S(PAD);

    int langCardTop = S(PAD);
    int langCardH = S(108);
    RECT langCard = { x0, langCardTop, x0 + cw, langCardTop + langCardH };
    FillRRAlpha(hdc, langCard, S(10), CLR_CARD, 190);
    StrokeRR(hdc, langCard, S(10), CLR_BORDER);

    {
        RECT tr = { langCard.left + S(14), langCard.top + S(12),
                    langCard.right - S(14), langCard.top + S(34) };
        TL(hdc, g_lang->langSection, tr, CLR_TEXT, g_fTitle);
    }

    for (int i = 0; i < NUM_LANGS; i++) {
        RECT btn = LangBtnRect(hw, i);
        bool sel = (i == g_selLang);
        bool hov = (g_hovLang == i);

        BYTE btnA = sel ? 210 : (hov ? 170 : 120);
        COLORREF bg = sel ? CLR_CARD_IN : (hov ? CLR_CARD_IN : CLR_BG);
        FillRRAlpha(hdc, btn, S(16), bg, btnA);

        if (sel || hov) {
            COLORREF bc = sel ? CLR_CYAN : CLR_BORDER;
            int bw = sel ? 2 : 1;
            StrokeRR(hdc, btn, S(16), bc, bw);
            if (sel) {
                InflateRect(&btn, S(1), S(1));
                StrokeRR(hdc, btn, S(17), RGB(0, 180, 200), 1);
            }
        }
        else StrokeRR(hdc, btn, S(16), CLR_BORDER);

        COLORREF tc = sel ? CLR_CYAN : CLR_TEXT;
        TL(hdc, g_langs[i]->langName, btn, tc, g_fNorm, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    int dlCardTop = langCardTop + langCardH + S(PAD);
    int dlCardH = H - fh - dlCardTop - S(PAD);
    RECT dlCard = { x0, dlCardTop, x0 + cw, dlCardTop + dlCardH };
    FillRRAlpha(hdc, dlCard, S(10), CLR_CARD, 190);
    StrokeRR(hdc, dlCard, S(10), CLR_BORDER);

    LONGLONG tot = g_totalSize;
    LONGLONG done = 0;
    for (int i = 0; i < NUM_THREADS; i++) done += g_ts[i].downloaded;
    int pct = (tot > 0) ? (int)(done * 100 / tot) : 0;

    std::wstring statusTxt = GetSt();
    if (statusTxt.empty()) statusTxt = g_lang->statusReady;
    if (!g_downloading && !g_allDone && statusTxt == g_lang->statusReady)
        statusTxt = g_lang->statusReady;

    {
        RECT sr = { dlCard.left + S(14), dlCard.top + S(14),
                    dlCard.right - S(60), dlCard.top + S(36) };
        TL(hdc, statusTxt.c_str(), sr, CLR_TEXT, g_fNorm);
        wchar_t pctBuf[16]; swprintf_s(pctBuf, 16, L"%d%%", pct);
        RECT pr = { dlCard.right - S(54), dlCard.top + S(14),
                    dlCard.right - S(14), dlCard.top + S(36) };
        TL(hdc, pctBuf, pr, CLR_TEXT, g_fNorm, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    int pbY = dlCard.top + S(44);
    int pbH = S(6);
    RECT pbBg = { dlCard.left + S(14), pbY, dlCard.right - S(14), pbY + pbH };
    FillRRAlpha(hdc, pbBg, pbH, CLR_PB_BG, 160);

    if (pct > 0) {
        int fw = (int)((pbBg.right - pbBg.left) * (float)pct / 100.f);
        if (fw < S(8)) fw = S(8);
        RECT pf = { pbBg.left, pbBg.top, pbBg.left + fw, pbBg.bottom };
        FillRRGradient(hdc, pf, pbH, CLR_PURPLE, CLR_CYAN);
    }
    else {
        RECT pf = { pbBg.left, pbBg.top, pbBg.left + S(40), pbBg.bottom };
        FillRRGradient(hdc, pf, pbH, CLR_PURPLE, CLR_CYAN);
    }

    {
        const wchar_t* sub = g_downloading ? g_lang->statusConn
            : (g_allDone && g_success ? g_lang->statusDone
                : (g_allDone ? g_lang->statusFail : g_lang->githubPending));
        RECT subR = { dlCard.left + S(14), pbY + pbH + S(8),
                      dlCard.right - S(14), pbY + pbH + S(26) };
        TL(hdc, sub, subR, CLR_MUTED, g_fSmall);
    }

    RECT dlBtn = DownloadBtnRect(hw);
    if (g_pressDl) {
        FillRR(hdc, dlBtn, S(10), CLR_PURPLE);
    }
    else {
        FillRRGradient(hdc, dlBtn, S(10), CLR_PURPLE, CLR_CYAN);
    }
    const wchar_t* dlLbl = g_allDone
        ? (g_success ? g_lang->btnLaunch : g_lang->btnRetry)
        : (g_downloading ? g_lang->btnLoading : g_lang->btnDownload);
    TC(hdc, dlLbl, dlBtn, CLR_TEXT, g_fBtn);

    RECT setBtn = SettingsBtnRect(hw);
    COLORREF setBg = g_pressSet ? CLR_CARD_IN : CLR_CARD;
    FillRRAlpha(hdc, setBtn, S(10), setBg, g_pressSet ? 200 : 150);
    StrokeRR(hdc, setBtn, S(10), g_hovSet ? CLR_SUB : CLR_BORDER, g_hovSet ? 2 : 1);
    TC(hdc, g_lang->btnSettings, setBtn, CLR_TEXT, g_fBtn);

    FillRAlpha(hdc, { 0, H - fh, W, H }, CLR_SIDEBAR, 175);
    {
        wchar_t footerLeft[256];
        swprintf_s(footerLeft, 256, L"System: %s. %s %s",
            g_osString.c_str(),
            g_hwOk ? g_lang->footerHwPassed : g_lang->footerHwFailed,
            g_online ? g_lang->footerConnOptimal : g_lang->footerConnOffline);
        RECT fl = { S(14), H - fh, W / 2 + S(80), H };
        TL(hdc, footerLeft, fl, CLR_SUB, g_fFooter, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        wchar_t thrBuf[48];
        swprintf_s(thrBuf, 48, g_lang->footerThreadsFmt, NUM_THREADS);
        int iconW = S(22);
        SIZE tsz{}; SelectObject(hdc, g_fFooter);
        GetTextExtentPoint32W(hdc, thrBuf, (int)wcslen(thrBuf), &tsz);
        int totalW = iconW + S(6) + tsz.cx + S(10);
        int rx = W - S(14) - totalW;
        int cy = H - fh / 2;
        DrawThreadsIcon(hdc, rx, cy - S(7), S(14), CLR_SUB);
        RECT tr = { rx + iconW + S(6), H - fh, W - S(22), H };
        TL(hdc, thrBuf, tr, CLR_SUB, g_fFooter, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        HBRUSH dot = CreateSolidBrush(CLR_CYAN);
        HPEN np = CreatePen(PS_NULL, 0, CLR_CYAN);
        HGDIOBJ ob = SelectObject(hdc, dot), op = SelectObject(hdc, np);
        Ellipse(hdc, W - S(18), cy - S(4), W - S(10), cy + S(4));
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(dot); DeleteObject(np);
    }

    // ── Сброс offscreen буфера на экран одним вызовом (без мерцания) ──
    BitBlt(hdc_orig, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, memOld);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hw, &ps);
}

static int HitLang(POINT pt, HWND hw)
{
    for (int i = 0; i < NUM_LANGS; i++) {
        RECT r = LangBtnRect(hw, i);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static void ShowSettings(HWND hw)
{
    MessageBoxW(hw, g_lang->settingsBody, g_lang->settingsTitle, MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wP, LPARAM lP)
{
    switch (msg)
    {
    case WM_PAINT:
        Paint(hw); return 0;
    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE: {
        int newSz = S(56);
        if (newSz != g_logoSz) {
            if (g_hLogo) DeleteObject(g_hLogo);
            g_hLogo = MakeLogoBmp(newSz);
            g_logoSz = newSz;
        }
        InvalidateRect(hw, nullptr, FALSE);
        return 0;
    }

    case WM_TIMER:
        InvalidateRect(hw, nullptr, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* mm = (MINMAXINFO*)lP;
        mm->ptMinTrackSize = { S(680), S(480) };
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lP), GET_Y_LPARAM(lP) };
        int li = HitLang(pt, hw);
        if (li >= 0) { g_pressLang = li; InvalidateRect(hw, nullptr, FALSE); return 0; }
        RECT dlR = DownloadBtnRect(hw), setR = SettingsBtnRect(hw);
        if (PtInRect(&dlR, pt)) { g_pressDl = true; InvalidateRect(hw, nullptr, FALSE); return 0; }
        if (PtInRect(&setR, pt)) { g_pressSet = true; InvalidateRect(hw, nullptr, FALSE); return 0; }
        return 0;
    }

    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lP), GET_Y_LPARAM(lP) };
        int prevLang = g_pressLang;
        g_pressLang = g_pressDl = g_pressSet = false;

        if (prevLang >= 0) {
            RECT r = LangBtnRect(hw, prevLang);
            if (PtInRect(&r, pt)) {
                g_selLang = prevLang;
                g_lang = g_langs[prevLang];
                SetSt(g_lang->statusReady);
            }
        }

        RECT dlR = DownloadBtnRect(hw), setR = SettingsBtnRect(hw);
        if (PtInRect(&setR, pt))
            ShowSettings(hw);

        if (PtInRect(&dlR, pt) && !g_downloading && (!g_allDone || !g_success)) {
            g_allDone = false; g_success = false; g_doneCnt = 0;
            for (int i = 0; i < NUM_THREADS; i++) {
                g_ts[i].downloaded = 0; g_ts[i].speed = 0;
                g_ts[i].done = false; g_ts[i].ok = false;
            }
            g_downloading = true;
            SetSt(g_lang->statusConn);
            g_online = CheckOnline();
            std::thread(DlMaster, hw).detach();
        }
        InvalidateRect(hw, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lP), GET_Y_LPARAM(lP) };
        g_hovLang = HitLang(pt, hw);
        RECT dlR = DownloadBtnRect(hw), setR = SettingsBtnRect(hw);
        g_hovDl = PtInRect(&dlR, pt) != FALSE;
        g_hovSet = PtInRect(&setR, pt) != FALSE;
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hw, 0 };
        TrackMouseEvent(&tme);
        bool hand = g_hovLang >= 0 || g_hovDl || g_hovSet;
        SetCursor(LoadCursor(nullptr, hand ? IDC_HAND : IDC_ARROW));
        InvalidateRect(hw, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_hovLang = -1; g_hovDl = g_hovSet = false;
        g_pressLang = g_pressDl = g_pressSet = false;
        InvalidateRect(hw, nullptr, FALSE);
        return 0;

    case WM_SETCURSOR:
        return TRUE;

    case WM_DL_PROGRESS:
        InvalidateRect(hw, nullptr, FALSE);
        return 0;

    case WM_DL_DONE: {
        g_downloading = false; g_allDone = true;
        bool ok = true;
        for (int i = 0; i < NUM_THREADS; i++) if (!g_ts[i].ok) { ok = false; break; }
        g_success = ok;
        if (ok) {
            SetSt(g_lang->statusDone);
            InvalidateRect(hw, nullptr, FALSE);
            Sleep(700);
            ShellExecuteW(nullptr, L"open", g_savePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            std::thread([hw, filePath = g_savePath]() {
                WatchAndDeleteFile(filePath);
                PostMessage(hw, WM_CLOSE, 0, 0);
            }).detach();
        }
        else {
            SetSt(g_lang->statusFail);
            InvalidateRect(hw, nullptr, FALSE);
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hw, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hw, msg, wP, lP);
}

// ── WinMain ──────────────────────────────────────────────────
int WINAPI wWinMain(_In_ HINSTANCE hI, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int nShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    { HDC dc = GetDC(nullptr); g_dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(nullptr, dc); }

    INITCOMMONCONTROLSEX ic{ sizeof(ic), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&ic);

    g_osString = QueryOsString();
    g_hwOk = CheckHardware();
    g_online = CheckOnline();

    MakeFonts();
    SetSt(kLangRU.statusReady);

    g_logoSz = S(56);
    g_hLogo = MakeLogoBmp(g_logoSz);

    HICON hIco = LoadIcon(hI, MAKEINTRESOURCE(101));
    if (!hIco) hIco = LoadIcon(nullptr, IDI_APPLICATION);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = 0; // CS_HREDRAW/CS_VREDRAW убраны — вызывали мерцание при ресайзе; WM_SIZE сам делает InvalidateRect
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"RVGInst";
    wc.hIcon = hIco;
    wc.hIconSm = hIco;
    RegisterClassExW(&wc);

    int winW = S(720), winH = S(520);
    wchar_t title[64];
    swprintf_s(title, 64, L"REvenge Installer %s", APP_VERSION);

    g_hWnd = CreateWindowExW(
        WS_EX_APPWINDOW, L"RVGInst", title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        (GetSystemMetrics(SM_CXSCREEN) - winW) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - winH) / 2,
        winW, winH, nullptr, nullptr, hI, nullptr);

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hWnd, 20, &dark, sizeof(dark));
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(g_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    COLORREF borderColor = CLR_CYAN;
    DwmSetWindowAttribute(g_hWnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    EnableAcrylicBackdrop(g_hWnd);

    ShowWindow(g_hWnd, nShow);
    UpdateWindow(g_hWnd);
    SetTimer(g_hWnd, 1, 1000, nullptr);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hLogo) DeleteObject(g_hLogo);
    FreeFonts();
    return (int)msg.wParam;
}
