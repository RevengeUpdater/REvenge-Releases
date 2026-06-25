// ============================================================
//  REvenge Installer  —  main.cpp (Fixed)
//  Win32 API + C++17  |  No external deps
//  Windows 10/11, Dark/Light auto, 4-thread DL, self-delete
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

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <memory>

#include "logo_data.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' "\
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "\
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

constexpr wchar_t DL_URL[]  = L"https://github.com/RevengeUpdater/REvenge-Releases/releases/download/v1.5.0/REvenge_Setup_1.5.0.exe";
constexpr wchar_t DL_NAME[] = L"REvenge_Setup_1.5.0.exe";
constexpr int NUM_THREADS   = 4;

constexpr UINT WM_DL_PROGRESS = WM_USER + 1;
constexpr UINT WM_DL_DONE     = WM_USER + 2;

// ── DPI ──────────────────────────────────────────────────────
static int g_dpi = 96;
inline int S(int v){ return MulDiv(v, g_dpi, 96); }

// ── Тема ─────────────────────────────────────────────────────
struct Theme {
    COLORREF bg, topbar, card, cardInner;
    COLORREF accent, accentHov, accentDim;
    COLORREF text, subtext, muted;
    COLORREF border, pbBg;
    bool dark;
};
static Theme g_t;

static void ApplyTheme()
{
    SYSTEMTIME st; GetLocalTime(&st);
    bool day = (st.wHour >= 6 && st.wHour < 20);
    if (day) {
        g_t = { 0xF0F0F0, 0xFFFFFF, 0xFFFFFF, 0xF7F7F7,
                0xCC2200, 0xAA1500, 0xFF4422,
                0x111111, 0x555555, 0xAAAAAA,
                0xDDDDDD, 0xE4E4E4, false };
    } else {
        g_t = { 0x181818, 0x242424, 0x2A2A2A, 0x222222,
                0xCC2200, 0xAA1500, 0xFF4422,
                0xEEEEEE, 0x888888, 0x4A4A4A,
                0x363636, 0x333333, true };
    }
}

// ── Состояние потока ────────────────────────────────────────
struct DownloadThread {
    LONGLONG downloaded;
    LONGLONG speed;
    bool done;
    bool ok;
    LONGLONG start;
    LONGLONG end;
    
    DownloadThread() : downloaded(0), speed(0), done(false), ok(false), start(0), end(0) {}
};

static DownloadThread       g_ts[NUM_THREADS];
static std::atomic<LONGLONG> g_totalSize(0);
static std::atomic<int>     g_doneCnt(0);
static std::atomic<bool>    g_downloading(false);
static std::atomic<bool>    g_allDone(false);
static std::atomic<bool>    g_success(false);
static std::wstring         g_savePath;
static std::wstring         g_status = L"Ready to download REvenge v1.5.0";
static std::mutex           g_stMtx;

static void SetSt(const std::wstring& s){
    std::lock_guard<std::mutex> lk(g_stMtx); g_status = s;
}
static std::wstring GetSt(){
    std::lock_guard<std::mutex> lk(g_stMtx); return g_status;
}

// ── Глобальные хэндлы ────────────────────────────────────────
static HWND    g_hWnd   = nullptr;
static HBITMAP g_hLogo  = nullptr;
static int     g_logoSz = 0;

// hover состояния
static bool g_hovClose=false, g_hovMin=false, g_hovMax=false;
static bool g_hovBtn=false, g_pressBtn=false;

// ── Шрифты ───────────────────────────────────────────────────
static HFONT g_fH1, g_fBold, g_fNorm, g_fSmall, g_fTiny, g_fMono, g_fBtn2, g_fItalic;

static void MakeFonts()
{
    auto MF=[](int pt,bool b,const wchar_t*f=L"Segoe UI")->HFONT{
        return CreateFont(S(-pt),0,0,0,b?FW_BOLD:FW_NORMAL,
            FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH|FF_SWISS,f);
    };
    g_fH1    = MF(18,true);    // Заголовок
    g_fBold  = MF(13,true);    // Основной текст
    g_fNorm  = MF(12,false);   // Обычный
    g_fSmall = MF(11,false);   // Мелкий
    g_fTiny  = MF(10,false);   // Footer
    g_fMono  = MF(11,false,L"Consolas");  // Путь
    g_fBtn2  = MF(15,true);    // Кнопка
    // Курсивный для статуса и ссылок
    g_fItalic = CreateFont(S(-11),0,0,0,FW_NORMAL,
        TRUE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
}

static void FreeFonts(){
    DeleteObject(g_fH1); DeleteObject(g_fBold);
    DeleteObject(g_fNorm); DeleteObject(g_fSmall);
    DeleteObject(g_fTiny); DeleteObject(g_fMono); 
    DeleteObject(g_fBtn2); DeleteObject(g_fItalic);
}

// ── Утилиты рисования ────────────────────────────────────────
static void FillR(HDC dc, RECT r, COLORREF c){
    HBRUSH b=CreateSolidBrush(c); FillRect(dc,&r,b); DeleteObject(b);
}
static void FillRR(HDC dc, RECT r, int rad, COLORREF c){
    HBRUSH b=CreateSolidBrush(c); HPEN p=CreatePen(PS_NULL,0,c);
    SelectObject(dc,b); SelectObject(dc,p);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,rad,rad);
    DeleteObject(b); DeleteObject(p);
}
static void StrokeRR(HDC dc,RECT r,int rad,COLORREF c,int w=1){
    SelectObject(dc,GetStockObject(NULL_BRUSH));
    HPEN p=CreatePen(PS_SOLID,w,c); HPEN op=(HPEN)SelectObject(dc,p);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,rad,rad);
    SelectObject(dc,op); DeleteObject(p);
}
static void TL(HDC dc,const wchar_t*s,RECT r,COLORREF c,HFONT f,
    UINT fl=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS){
    SelectObject(dc,f); SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,c); DrawText(dc,s,-1,&r,fl);
}
static void TC(HDC dc,const wchar_t*s,RECT r,COLORREF c,HFONT f){
    TL(dc,s,r,c,f,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
static void HLine(HDC dc,int x1,int x2,int y,COLORREF c){
    HPEN p=CreatePen(PS_SOLID,1,c); HPEN o=(HPEN)SelectObject(dc,p);
    MoveToEx(dc,x1,y,nullptr); LineTo(dc,x2,y);
    SelectObject(dc,o); DeleteObject(p);
}
static void Circle(HDC dc,int cx,int cy,int r,COLORREF c){
    HBRUSH b=CreateSolidBrush(c); HPEN p=CreatePen(PS_NULL,0,c);
    SelectObject(dc,b); SelectObject(dc,p);
    Ellipse(dc,cx-r,cy-r,cx+r,cy+r);
    DeleteObject(b); DeleteObject(p);
}

// ── Форматирование ───────────────────────────────────────────
static std::wstring FmtSpd(LONGLONG b){
    wchar_t buf[32];
    if(b<1024)      swprintf_s(buf,32,L"%lld B/s",b);
    else if(b<1<<20) swprintf_s(buf,32,L"%.1f KB/s",b/1024.0);
    else             swprintf_s(buf,32,L"%.1f MB/s",b/1048576.0);
    return std::wstring(buf);
}
static std::wstring FmtMB(LONGLONG b){
    wchar_t buf[32];
    swprintf_s(buf,32,L"%.1f MB",b/1048576.0);
    return std::wstring(buf);
}

// ── Логотип из BGRA ──────────────────────────────────────────
static HBITMAP MakeLogoBmp(int sz)
{
    HDC hdc=GetDC(nullptr);
    HDC src=CreateCompatibleDC(hdc);
    HDC dst=CreateCompatibleDC(hdc);

    BITMAPINFO bi{}; bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=LOGO_W; bi.bmiHeader.biHeight=-LOGO_H;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;

    void* p=nullptr;
    HBITMAP hS=CreateDIBSection(src,&bi,DIB_RGB_COLORS,&p,nullptr,0);
    if(hS&&p) memcpy(p,LOGO_BGRA,LOGO_W*LOGO_H*4);

    bi.bmiHeader.biWidth=sz; bi.bmiHeader.biHeight=-sz;
    void* pd=nullptr;
    HBITMAP hD=CreateDIBSection(dst,&bi,DIB_RGB_COLORS,&pd,nullptr,0);

    HBITMAP oS=(HBITMAP)SelectObject(src,hS);
    HBITMAP oD=(HBITMAP)SelectObject(dst,hD);
    SetStretchBltMode(dst,HALFTONE);
    StretchBlt(dst,0,0,sz,sz,src,0,0,LOGO_W,LOGO_H,SRCCOPY);
    SelectObject(src,oS); SelectObject(dst,oD);
    DeleteDC(src); DeleteDC(dst); DeleteObject(hS);
    ReleaseDC(nullptr,hdc);
    return hD;
}

static void DrawLogo(HDC dc,int x,int y,int sz)
{
    if(!g_hLogo) return;
    HDC mem=CreateCompatibleDC(dc);
    HBITMAP old=(HBITMAP)SelectObject(mem,g_hLogo);
    BLENDFUNCTION bf; bf.BlendOp=AC_SRC_OVER; bf.BlendFlags=0;
    bf.SourceConstantAlpha=255; bf.AlphaFormat=AC_SRC_ALPHA;
    AlphaBlend(dc,x,y,sz,sz,mem,0,0,sz,sz,bf);
    SelectObject(mem,old); DeleteDC(mem);
}

// ── Удаление файла через cmd.exe (отдельный процесс) ────────
// Слежение за процессом REvenge_Setup и удаление файла после закрытия
static void WatchAndDeleteFile(const std::wstring& filePath)
{
    // Ждём 2 сек чтобы процесс успел запуститься
    Sleep(2000);
    
    // Ищем процесс REvenge_Setup_1.5.0.exe в цикле (до 60 сек)
    DWORD targetPid = 0;
    for(int attempt = 0; attempt < 60 && targetPid == 0; attempt++){
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if(snap != INVALID_HANDLE_VALUE){
            PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
            if(Process32FirstW(snap, &pe)){
                do {
                    if(_wcsicmp(pe.szExeFile, L"REvenge_Setup_1.5.0.exe") == 0){
                        targetPid = pe.th32ProcessID;
                        break;
                    }
                } while(Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }
        if(!targetPid) Sleep(1000);
    }
    
    // Если нашли процесс - ждём его закрытия
    if(targetPid != 0){
        HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, targetPid);
        if(hProc){
            WaitForSingleObject(hProc, INFINITE);  // Ждём пока процесс закроется
            CloseHandle(hProc);
        }
    } else {
        // Если не нашли - ждём 60 сек и всё равно удаляем
        Sleep(60000);
    }
    
    // Удаляем файл (даём ему 2 сек чтобы ОС отпустила блокировку)
    Sleep(2000);
    DeleteFileW(filePath.c_str());
}


// ── Поток загрузки диапазона ─────────────────────────────────
static void DlRange(int idx, HWND hWnd)
{
    DownloadThread& ts = g_ts[idx];
    
    HINTERNET hI=InternetOpenW(L"RVG/1.5",INTERNET_OPEN_TYPE_PRECONFIG,nullptr,nullptr,0);
    if(!hI){ ts.done=true; ts.ok=false;
        if(++g_doneCnt==NUM_THREADS) PostMessage(hWnd,WM_DL_DONE,0,0); return; }

    wchar_t hdr[128];
    swprintf_s(hdr,128,L"Range: bytes=%lld-%lld\r\n",ts.start,ts.end);
    
    HINTERNET hU=InternetOpenUrlW(hI,DL_URL,hdr,(DWORD)wcslen(hdr),
        INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE|
        INTERNET_FLAG_SECURE|INTERNET_FLAG_IGNORE_CERT_CN_INVALID,0);
    if(!hU){ InternetCloseHandle(hI);
        ts.done=true; ts.ok=false;
        if(++g_doneCnt==NUM_THREADS) PostMessage(hWnd,WM_DL_DONE,0,0); return; }

    HANDLE hF=CreateFileW(g_savePath.c_str(),GENERIC_WRITE,FILE_SHARE_WRITE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(hF==INVALID_HANDLE_VALUE){
        InternetCloseHandle(hU); InternetCloseHandle(hI);
        ts.done=true; ts.ok=false;
        if(++g_doneCnt==NUM_THREADS) PostMessage(hWnd,WM_DL_DONE,0,0); return; }

    LARGE_INTEGER li; li.QuadPart=ts.start;
    SetFilePointerEx(hF,li,nullptr,FILE_BEGIN);

    constexpr DWORD CHUNK=131072;
    auto buf=std::make_unique<BYTE[]>(CHUNK);
    DWORD lastTick=GetTickCount(); LONGLONG lastB=0;

    while(true){
        DWORD r=0;
        if(!InternetReadFile(hU,buf.get(),CHUNK,&r)||r==0) break;
        DWORD w=0; WriteFile(hF,buf.get(),r,&w,nullptr);
        ts.downloaded+=r;
        DWORD now=GetTickCount(), el=now-lastTick;
        if(el>=400){
            ts.speed=(ts.downloaded-lastB)*1000/el;
            lastTick=now; lastB=ts.downloaded;
        }
        PostMessage(hWnd,WM_DL_PROGRESS,0,0);
    }
    CloseHandle(hF);
    InternetCloseHandle(hU); InternetCloseHandle(hI);
    ts.ok=true; ts.done=true;
    if(++g_doneCnt==NUM_THREADS) PostMessage(hWnd,WM_DL_DONE,1,0);
}

// ── Мастер-поток ─────────────────────────────────────────────
static void DlMaster(HWND hWnd)
{
    wchar_t dl[MAX_PATH]={};
    if(FAILED(SHGetFolderPathW(nullptr,CSIDL_PERSONAL,nullptr,0,dl)))
        GetTempPathW(MAX_PATH,dl);
    wchar_t tmp[MAX_PATH]; wcscpy_s(tmp,MAX_PATH,dl); wcscat_s(tmp,MAX_PATH,L"\\..\\Downloads");
    wchar_t res[MAX_PATH]; GetFullPathNameW(tmp,MAX_PATH,res,nullptr);
    if(GetFileAttributesW(res)!=INVALID_FILE_ATTRIBUTES) wcscpy_s(dl,MAX_PATH,res);
    g_savePath=std::wstring(dl)+L"\\"+DL_NAME;

    HINTERNET hI=InternetOpenW(L"RVG/1.5",INTERNET_OPEN_TYPE_PRECONFIG,nullptr,nullptr,0);
    HINTERNET hU=InternetOpenUrlW(hI,DL_URL,nullptr,0,
        INTERNET_FLAG_RELOAD|INTERNET_FLAG_SECURE|INTERNET_FLAG_IGNORE_CERT_CN_INVALID,0);
    LONGLONG fSz=0;
    if(hU){
        wchar_t lb[32]={}; DWORD ls=sizeof(lb);
        if(HttpQueryInfoW(hU,HTTP_QUERY_CONTENT_LENGTH,lb,&ls,nullptr))
            fSz=_wtoll(lb);
        InternetCloseHandle(hU);
    }
    if(hI) InternetCloseHandle(hI);
    g_totalSize=fSz;

    HANDLE hF=CreateFileW(g_savePath.c_str(),GENERIC_WRITE,FILE_SHARE_WRITE,
        nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(hF==INVALID_HANDLE_VALUE){ PostMessage(hWnd,WM_DL_DONE,0,0); return; }
    if(fSz>0){
        LARGE_INTEGER li; li.QuadPart=fSz-1;
        SetFilePointerEx(hF,li,nullptr,FILE_BEGIN);
        BYTE z=0; DWORD w; WriteFile(hF,&z,1,&w,nullptr);
    }
    CloseHandle(hF);

    LONGLONG chunk=fSz>0?fSz/NUM_THREADS:0;
    for(int i=0;i<NUM_THREADS;i++){
        g_ts[i].downloaded=0; g_ts[i].speed=0;
        g_ts[i].done=false; g_ts[i].ok=false;
        g_ts[i].start=i*chunk;
        g_ts[i].end=(i==NUM_THREADS-1)?(fSz>0?fSz-1:0):(i+1)*chunk-1;
        std::thread(DlRange,i,hWnd).detach();
    }
}

// ── PAINT ─────────────────────────────────────────────────────
static void Paint(HWND hw)
{
    PAINTSTRUCT ps; HDC hdc=BeginPaint(hw,&ps);
    RECT rc; GetClientRect(hw,&rc);
    int W=rc.right, H=rc.bottom;

    HDC m=CreateCompatibleDC(hdc);
    HBITMAP bm=CreateCompatibleBitmap(hdc,W,H);
    SelectObject(m,bm);

    FillR(m,rc,g_t.bg);

    // Topbar
    int TH=S(46);
    FillR(m,{0,0,W,TH},g_t.topbar);
    HLine(m,0,W,TH,g_t.border);

    int by=TH/2, bx=S(18), br=S(7);
    Circle(m,bx,       by,br, g_hovClose ? 0x3388FF : 0x2DCA56);
    Circle(m,bx+S(22), by,br, g_hovMin   ? 0x3388FF : 0xF5BC2F);
    Circle(m,bx+S(44), by,br, g_hovMax   ? 0x3388FF : 0xFF5F57);

    { RECT r={S(80),0,W-S(80),TH}; TC(m,L"REvenge Installer",r,g_t.text,g_fBold); }

    int lsz=S(34);
    if(g_hLogo) DrawLogo(m, W-lsz-S(12), (TH-lsz)/2, lsz);

    // Download card
    int pad=S(14);
    int cY=TH+pad;
    int cH=S(128);
    RECT card={pad,cY,W-pad,cY+cH};
    FillRR(m,card,S(10),g_t.card);
    StrokeRR(m,card,S(10),g_t.border);

    { RECT r={card.left+S(14),card.top+S(10),card.right-S(14),card.top+S(32)};
      TL(m,L"Download Package",r,g_t.text,g_fBold); }

    { std::wstring st=GetSt();
      RECT r={card.left+S(14),card.top+S(30),card.right-S(14),card.top+S(50)};
      TL(m,st.c_str(),r,g_t.accent,g_fItalic); }

    HLine(m,card.left+S(10),card.right-S(10),card.top+S(52),g_t.border);

    { RECT r={card.left+S(14),card.top+S(56),card.right-S(14),card.top+S(74)};
      TL(m,DL_NAME,r,g_t.text,g_fMono); }
    { RECT r={card.left+S(14),card.top+S(74),card.right-S(14),card.top+S(90)};
      TL(m,L"github.com/RevengeUpdater/REvenge-Releases",r,g_t.muted,g_fItalic); }

    LONGLONG tot=g_totalSize;
    LONGLONG done2=0; for(int i=0;i<NUM_THREADS;i++) done2+=g_ts[i].downloaded;
    int pct=(tot>0)?(int)(done2*100/tot):0;

    int pbY=card.top+S(96), pbH=S(8);
    RECT pbBg={card.left+S(14),pbY,card.right-S(14),pbY+pbH};
    FillRR(m,pbBg,pbH/2,g_t.pbBg);
    if(pct>0){
        int fw=(int)((pbBg.right-pbBg.left)*(float)pct/100.f);
        if(fw>=pbH){ RECT pf={pbBg.left,pbBg.top,pbBg.left+fw,pbBg.bottom};
            FillRR(m,pf,pbH/2,g_t.accent); }
    }

    { wchar_t buf[16]; swprintf_s(buf,16,L"%d%%",pct);
      RECT r={card.right-S(44),pbY-S(2),card.right-S(10),pbY+pbH+S(2)};
      TC(m,buf,r,g_t.accent,g_fTiny); }
    if(tot>0){
        std::wstring szTxt = FmtMB(done2)+L" / "+FmtMB(tot);
        RECT r={card.left+S(14),pbY-S(1),card.right-S(50),pbY+pbH+S(2)};
        TL(m,szTxt.c_str(),r,g_t.muted,g_fTiny); }

    if(g_downloading){
        LONGLONG totalSpd=0; for(int i=0;i<NUM_THREADS;i++) totalSpd+=g_ts[i].speed;
        std::wstring spdTxt = L"Total: "+FmtSpd(totalSpd);
        RECT r={card.left+S(14),card.top+S(110),card.right-S(14),card.top+cH-S(4)};
        TL(m,spdTxt.c_str(),r,g_t.subtext,g_fTiny);
    }

    // Thread cards
    int tY=cY+cH+pad;
    int tCardH=S(70);
    int gap=S(8);
    int tW=(W-pad*2-gap*(NUM_THREADS-1))/NUM_THREADS;

    for(int i=0;i<NUM_THREADS;i++){
        int tx=pad+i*(tW+gap);
        RECT tc={tx,tY,tx+tW,tY+tCardH};

        COLORREF borderClr = g_ts[i].done
            ? (g_ts[i].ok ? g_t.accent : 0xAA3333)
            : g_t.border;
        FillRR(m,tc,S(8),g_t.card);
        StrokeRR(m,tc,S(8),borderClr, g_ts[i].done ? 2 : 1);

        { wchar_t thn[16]; swprintf_s(thn,16,L"Thread %d",i+1);
          RECT rh={tc.left+S(8),tc.top+S(6),tc.right-S(6),tc.top+S(20)};
          TL(m,thn,rh,g_t.muted,g_fTiny); }

        { wchar_t sp[32];
          if(g_ts[i].done)      wcscpy_s(sp,32,g_ts[i].ok?L"Done":L"Error");
          else if(g_downloading) wcscpy_s(sp,32,FmtSpd(g_ts[i].speed).c_str());
          else                   wcscpy_s(sp,32,L"Waiting");
          RECT rs={tc.left+S(8),tc.top+S(20),tc.right-S(6),tc.top+S(42)};
          TL(m,sp,rs,g_t.text,g_fBold); }

        { LONGLONG rng=g_ts[i].end-g_ts[i].start+1;
          int tp=(rng>0)?(int)(g_ts[i].downloaded*100/rng):0;
          if(!g_downloading&&!g_allDone) tp=0;
          wchar_t pcs[8]; swprintf_s(pcs,8,L"%d%%",tp);
          RECT rp={tc.right-S(32),tc.top+S(20),tc.right-S(4),tc.top+S(42)};
          TL(m,pcs,rp,g_t.accent,g_fTiny,DT_RIGHT|DT_VCENTER|DT_SINGLELINE); }

        { int mpY=tc.top+S(50), mpH=S(4);
          RECT mpB={tc.left+S(8),mpY,tc.right-S(8),mpY+mpH};
          FillRR(m,mpB,mpH/2,g_t.pbBg);
          LONGLONG rng=g_ts[i].end-g_ts[i].start+1;
          int tp=(rng>0)?(int)(g_ts[i].downloaded*100/rng):0;
          if(tp>0){
            int fw2=(int)((mpB.right-mpB.left)*(float)tp/100.f);
            if(fw2>=mpH){
                RECT mpF={mpB.left,mpB.top,mpB.left+fw2,mpB.bottom};
                FillRR(m,mpF,mpH/2,g_ts[i].done&&!g_ts[i].ok?0xAA3333:g_t.accent);
            }
          }
        }
    }

    // Download button
    int btnH=S(52);
    int btnY2=H-btnH-S(16);
    RECT btnR={pad,btnY2,W-pad,btnY2+btnH};

    COLORREF bc=g_t.accent;
    if(g_pressBtn)  bc=g_t.accentDim;
    else if(g_hovBtn) bc=g_t.accentHov;

    FillRR(m,btnR,S(12),bc);

    if(!g_pressBtn&&!g_allDone){
        int rr=(int)GetRValue(bc)+50; if(rr>255)rr=255;
        int gg=(int)GetGValue(bc)+50; if(gg>255)gg=255;
        int bb=(int)GetBValue(bc)+50; if(bb>255)bb=255;
        RECT hi={btnR.left+S(30),btnR.top+S(1),btnR.right-S(30),btnR.top+S(3)};
        FillRR(m,hi,2,RGB(rr,gg,bb));
    }

    const wchar_t* lbl = g_allDone
        ? (g_success ? L"Launching installer..." : L"Download Failed  -  Retry")
        : (g_downloading ? L"Downloading..." : L"Download  &  Install");
    TC(m,lbl,btnR,0xFFFFFF,g_fBtn2);

    // Footer
    SYSTEMTIME ft; GetLocalTime(&ft);
    wchar_t footerTxt[128];
    swprintf_s(footerTxt,128,
        L"REvenge v1.5.0   %d threads   %02d:%02d:%02d   %s mode",
        NUM_THREADS, ft.wHour, ft.wMinute, ft.wSecond,
        g_t.dark ? L"Dark" : L"Light");
    RECT rf={pad,H-S(18),W-pad,H-S(2)};
    TC(m,footerTxt,rf,g_t.muted,g_fTiny);

    BitBlt(hdc,0,0,W,H,m,0,0,SRCCOPY);
    DeleteObject(bm); DeleteDC(m);
    EndPaint(hw,&ps);
}

// ── Хит-тест traffic lights ──────────────────────────────────
static int HitTL(POINT pt)
{
    int by=S(46)/2, bx=S(18), br=S(10);
    auto ic=[&](int cx,int cy){ int dx=pt.x-cx,dy=pt.y-cy; return dx*dx+dy*dy<=br*br; };
    if(ic(bx,by))       return 1;
    if(ic(bx+S(22),by)) return 2;
    if(ic(bx+S(44),by)) return 3;
    return 0;
}

static RECT GetBtnRect(HWND hw)
{
    RECT rc; GetClientRect(hw,&rc);
    int H=rc.bottom, pad=S(14);
    int btnH=S(52), btnY=H-btnH-S(16);
    return {pad, btnY, rc.right-pad, btnY+btnH};
}

// ── WndProc ───────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wP,LPARAM lP)
{
    switch(msg)
    {
    case WM_PAINT: Paint(hw); return 0;
    case WM_ERASEBKGND: return 1;

    case WM_SIZE: {
        int newSz=S(34);
        if(newSz!=g_logoSz){
            if(g_hLogo) DeleteObject(g_hLogo);
            g_hLogo=MakeLogoBmp(newSz);
            g_logoSz=newSz;
        }
        InvalidateRect(hw,nullptr,FALSE);
        return 0;
    }

    case WM_TIMER:
        InvalidateRect(hw,nullptr,FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        auto*mm=(MINMAXINFO*)lP;
        mm->ptMinTrackSize={S(440),S(360)};
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt={GET_X_LPARAM(lP),GET_Y_LPARAM(lP)};
        RECT br=GetBtnRect(hw);
        if(PtInRect(&br,pt)){
            g_pressBtn=true; InvalidateRect(hw,nullptr,FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        POINT pt={GET_X_LPARAM(lP),GET_Y_LPARAM(lP)};
        g_pressBtn=false;

        int hit=HitTL(pt);
        if(hit==1){ PostMessage(hw,WM_CLOSE,0,0); return 0; }
        if(hit==2){ ShowWindow(hw,SW_MINIMIZE); return 0; }
        if(hit==3){
            WINDOWPLACEMENT wp{sizeof(wp)}; GetWindowPlacement(hw,&wp);
            ShowWindow(hw,wp.showCmd==SW_MAXIMIZE?SW_RESTORE:SW_MAXIMIZE);
            return 0;
        }

        RECT br=GetBtnRect(hw);
        if(PtInRect(&br,pt)&&!g_downloading&&(!g_allDone||!g_success)){
            g_allDone=false; g_success=false; g_doneCnt=0;
            for(int i=0;i<NUM_THREADS;i++){
                g_ts[i].downloaded=0; g_ts[i].speed=0;
                g_ts[i].done=false; g_ts[i].ok=false;
            }
            g_downloading=true;
            SetSt(L"Connecting to GitHub...");
            InvalidateRect(hw,nullptr,FALSE);
            std::thread(DlMaster,hw).detach();
        }
        InvalidateRect(hw,nullptr,FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt={GET_X_LPARAM(lP),GET_Y_LPARAM(lP)};
        int hit=HitTL(pt);
        g_hovClose=(hit==1); g_hovMin=(hit==2); g_hovMax=(hit==3);

        RECT br=GetBtnRect(hw);
        g_hovBtn=PtInRect(&br,pt)!=FALSE;

        TRACKMOUSEEVENT tme{sizeof(tme),TME_LEAVE,hw,0};
        TrackMouseEvent(&tme);
        SetCursor(LoadCursor(nullptr,(hit>0||g_hovBtn)?IDC_HAND:IDC_ARROW));
        InvalidateRect(hw,nullptr,FALSE);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_hovClose=g_hovMin=g_hovMax=g_hovBtn=g_pressBtn=false;
        InvalidateRect(hw,nullptr,FALSE);
        return 0;
    case WM_SETCURSOR: return TRUE;

    case WM_DL_PROGRESS:
        InvalidateRect(hw,nullptr,FALSE);
        return 0;

    case WM_DL_DONE: {
        g_downloading=false; g_allDone=true;
        bool ok=true;
        for(int i=0;i<NUM_THREADS;i++) if(!g_ts[i].ok){ok=false;break;}
        g_success=ok;
        if(ok){
            SetSt(L"Download complete - launching installer...");
            InvalidateRect(hw,nullptr,FALSE);
            Sleep(700);
            ShellExecuteW(nullptr,L"open",g_savePath.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            // Запускаем слежение и закрытие приложения в отдельном потоке
            // чтобы поток слежения успел запуститься до закрытия процесса
            std::thread([hw,filePath=g_savePath](){
                WatchAndDeleteFile(filePath);
                PostMessage(hw, WM_CLOSE, 0, 0);
            }).detach();
            // НЕ закрываем сразу - даём потоку время запуститься
        } else {
            SetSt(L"Some threads failed. Click to retry.");
            InvalidateRect(hw,nullptr,FALSE);
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hw,&pt);
        RECT rc2; GetClientRect(hw,&rc2);
        int e=S(5);
        if(pt.x<e&&pt.y<e)               return HTTOPLEFT;
        if(pt.x>rc2.right-e&&pt.y<e)     return HTTOPRIGHT;
        if(pt.x<e&&pt.y>rc2.bottom-e)    return HTBOTTOMLEFT;
        if(pt.x>rc2.right-e&&pt.y>rc2.bottom-e) return HTBOTTOMRIGHT;
        if(pt.x<e)                        return HTLEFT;
        if(pt.x>rc2.right-e)             return HTRIGHT;
        if(pt.y<e)                        return HTTOP;
        if(pt.y>rc2.bottom-e)            return HTBOTTOM;
        if(pt.y<S(46)&&HitTL(pt)==0)     return HTCAPTION;
        return HTCLIENT;
    }

    case WM_DESTROY:
        KillTimer(hw,1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hw,msg,wP,lP);
}

// ── WinMain ───────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hI,HINSTANCE,PWSTR,int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    { HDC dc=GetDC(nullptr); g_dpi=GetDeviceCaps(dc,LOGPIXELSX); ReleaseDC(nullptr,dc); }

    INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_WIN95_CLASSES};
    InitCommonControlsEx(&ic);

    ApplyTheme();
    MakeFonts();

    g_logoSz=S(34);
    g_hLogo=MakeLogoBmp(g_logoSz);

    HICON hIco=LoadIcon(hI,MAKEINTRESOURCE(101));
    if(!hIco) hIco=LoadIcon(nullptr,IDI_APPLICATION);

    WNDCLASSEXW wc{};
    wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc; wc.hInstance=hI;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName=L"RVGInst";
    wc.hIcon=hIco; wc.hIconSm=hIco;
    RegisterClassExW(&wc);

    int W=S(580),H=S(400);
    g_hWnd=CreateWindowExW(WS_EX_APPWINDOW,L"RVGInst",L"REvenge Installer",
        WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN,
        (GetSystemMetrics(SM_CXSCREEN)-W)/2,
        (GetSystemMetrics(SM_CYSCREEN)-H)/2,
        W,H,nullptr,nullptr,hI,nullptr);

    BOOL dark=g_t.dark?TRUE:FALSE;
    DwmSetWindowAttribute(g_hWnd,20,&dark,sizeof(dark));
    DWM_WINDOW_CORNER_PREFERENCE corner=DWMWCP_ROUND;
    DwmSetWindowAttribute(g_hWnd,DWMWA_WINDOW_CORNER_PREFERENCE,&corner,sizeof(corner));
    MARGINS mg={1,1,1,1};
    DwmExtendFrameIntoClientArea(g_hWnd,&mg);

    ShowWindow(g_hWnd,SW_SHOW);
    UpdateWindow(g_hWnd);
    SetTimer(g_hWnd,1,1000,nullptr);

    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)){
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }

    if(g_hLogo) DeleteObject(g_hLogo);
    FreeFonts();
    return (int)msg.wParam;
}
