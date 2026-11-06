// shot - PrintScreen'e bagli, arkaplanda calisan minik ekran goruntusu araci.
//
//   shot            : arkaplanda baslatir (zaten calisiyorsa hicbir sey yapmaz)
//   shot -startup   : Windows ile birlikte baslayacak sekilde kaydeder (+ simdi baslatir)
//   shot -nostartup : otomatik baslatmayi kaldirir
//   shot -exit      : calisan kopyayi kapatir
//   shot -f         : tam ekran goruntusu (panoya + Resimler\*.jpg, kalite %50)
//   shot -w         : en one alinmis pencerenin goruntusu
//
//   Kisayollar (arkaplan kopyasi calisirken):  PrintScreen = -f,  Shift+PrintScreen = -w
//
// Derleme: build.bat   (MSVC, CRT'siz, birkac KB)

#include <windows.h>
#include <shlobj.h>
#include <objidl.h>
#include <gdiplus.h>

#pragma comment(linker, "/ENTRY:entry /SUBSYSTEM:WINDOWS")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

// ---- CRT yok: derleyicinin ihtiyac duyabilecegi iki fonksiyon ----
extern "C" {
#pragma function(memset, memcpy)
void* memset(void* d, int c, size_t n) { unsigned char* p = (unsigned char*)d; while (n--) *p++ = (unsigned char)c; return d; }
void* memcpy(void* d, const void* s, size_t n) { unsigned char* p = (unsigned char*)d; const unsigned char* q = (const unsigned char*)s; while (n--) *p++ = *q++; return d; }
}

static const wchar_t kClass[]   = L"shot_msg_window";
static const wchar_t kMutex[]   = L"Local\\shot_single_instance";
static const wchar_t kRunKey[]  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t kRunName[] = L"shot";
static const int     kQuality   = 50;     // JPG kalitesi (%50)

// DwmGetWindowAttribute (dwmapi) - DWMWA_EXTENDED_FRAME_BOUNDS = 9
extern "C" HRESULT WINAPI DwmGetWindowAttribute(HWND, DWORD, PVOID, DWORD);

// ------------------------------------------------------------------
// Yakalama
// ------------------------------------------------------------------
static bool SaveJpeg(const BITMAPINFO* bi, void* bits, const wchar_t* path)
{
    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &in, NULL) != Gdiplus::Ok) return false;

    bool ok = false;
    Gdiplus::GpBitmap* bmp = NULL;
    if (Gdiplus::DllExports::GdipCreateBitmapFromGdiDib(bi, bits, &bmp) == Gdiplus::Ok && bmp) {
        // image/jpeg kodlayici ve kalite parametresi (sabit GUID'ler)
        static const CLSID jpegClsid   = { 0x557cf401, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
        static const GUID  qualityGuid = { 0x1D5BE4B5, 0xFA4A, 0x452D, { 0x9C, 0xDD, 0x5D, 0xB3, 0x51, 0x05, 0xE7, 0xEB } };
        ULONG q = kQuality;
        Gdiplus::EncoderParameters ep;
        ep.Count = 1;
        ep.Parameter[0].Guid = qualityGuid;
        ep.Parameter[0].NumberOfValues = 1;
        ep.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        ep.Parameter[0].Value = &q;
        ok = Gdiplus::DllExports::GdipSaveImageToFile((Gdiplus::GpImage*)bmp, path, &jpegClsid, &ep) == Gdiplus::Ok;
        Gdiplus::DllExports::GdipDisposeImage((Gdiplus::GpImage*)bmp);
    }
    Gdiplus::GdiplusShutdown(token);
    return ok;
}

static void CopyToClipboard(const BITMAPINFOHEADER* hdr, const void* bits, SIZE_T bitsSize)
{
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + bitsSize);
    if (!g) return;
    void* p = GlobalLock(g);
    if (!p) { GlobalFree(g); return; }
    memcpy(p, hdr, sizeof(BITMAPINFOHEADER));
    memcpy((BYTE*)p + sizeof(BITMAPINFOHEADER), bits, bitsSize);
    GlobalUnlock(g);

    bool opened = false;
    for (int i = 0; i < 20 && !opened; i++) {            // pano baska uygulamada kilitli olabilir
        opened = OpenClipboard(NULL) != 0;
        if (!opened) Sleep(15);
    }
    if (!opened) { GlobalFree(g); return; }
    EmptyClipboard();
    if (!SetClipboardData(CF_DIB, g)) GlobalFree(g);     // basarida sahiplik sisteme gecer
    CloseClipboard();
}

static void Capture(bool activeWindow)
{
    RECT vs;                                              // tum monitorleri kapsayan sanal ekran
    vs.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
    vs.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
    vs.right  = vs.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    vs.bottom = vs.top  + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    RECT r = vs;
    if (activeWindow) {
        HWND fg = GetForegroundWindow();
        RECT wr;
        if (fg && !IsIconic(fg)) {
            if (DwmGetWindowAttribute(fg, 9, &wr, sizeof(wr)) != S_OK && !GetWindowRect(fg, &wr))
                wr = vs;
            // sanal ekranla kesistir (maximize pencerelerin tasan golgesi vb.)
            if (wr.left   < vs.left)   wr.left   = vs.left;
            if (wr.top    < vs.top)    wr.top    = vs.top;
            if (wr.right  > vs.right)  wr.right  = vs.right;
            if (wr.bottom > vs.bottom) wr.bottom = vs.bottom;
            if (wr.right > wr.left && wr.bottom > wr.top) r = wr;
        }
    }
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;

    HDC sdc = GetDC(NULL);
    HDC mdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = h;                       // asagidan yukari
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    SIZE_T stride = ((SIZE_T)w * 3 + 3) & ~(SIZE_T)3;
    bi.bmiHeader.biSizeImage   = (DWORD)(stride * h);

    void* bits = NULL;
    HBITMAP hb = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hb && bits) {
        HGDIOBJ old = SelectObject(mdc, hb);
        BitBlt(mdc, 0, 0, w, h, sdc, r.left, r.top, SRCCOPY | CAPTUREBLT);
        SelectObject(mdc, old);
        GdiFlush();

        // 1) panoya
        CopyToClipboard(&bi.bmiHeader, bits, stride * h);

        // 2) Resimler\shot_YYYYMMDD_HHMMSS_mmm.jpg
        wchar_t dir[MAX_PATH], path[MAX_PATH + 64];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYPICTURES | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, dir))) {
            SYSTEMTIME t;
            GetLocalTime(&t);
            wsprintfW(path, L"%s\\shot_%04d%02d%02d_%02d%02d%02d_%03d.jpg",
                      dir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
            SaveJpeg(&bi, bits, path);
        }
    }
    if (hb) DeleteObject(hb);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
}

// ------------------------------------------------------------------
// Arkaplan penceresi (gorunmez, mesaj-only) ve kisayollar
// ------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_HOTKEY:
        Capture(w == 2);                                  // 1 = PrintScreen, 2 = Shift+PrintScreen
        return 0;
    case WM_DESTROY:
        UnregisterHotKey(h, 1);
        UnregisterHotKey(h, 2);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static HWND FindRunning()
{
    return FindWindowExW(HWND_MESSAGE, NULL, kClass, NULL);
}

static int RunDaemon(HINSTANCE hi)
{
    HANDLE mtx = CreateMutexW(NULL, FALSE, kMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;   // zaten calisiyor

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc)) return 1;
    HWND hwnd = CreateWindowExW(0, kClass, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hi, NULL);
    if (!hwnd) return 1;

    RegisterHotKey(hwnd, 1, MOD_NOREPEAT,             VK_SNAPSHOT);
    RegisterHotKey(hwnd, 2, MOD_NOREPEAT | MOD_SHIFT, VK_SNAPSHOT);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (mtx) CloseHandle(mtx);
    return 0;
}

// ------------------------------------------------------------------
// Komutlar
// ------------------------------------------------------------------
static void StartDaemonProcess()
{
    if (FindRunning()) return;
    wchar_t exe[MAX_PATH + 2];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static void SetStartup(bool on)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t exe[MAX_PATH], cmd[MAX_PATH + 4];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        wsprintfW(cmd, L"\"%s\"", exe);
        DWORD n = 0;
        while (cmd[n]) n++;
        RegSetValueExW(k, kRunName, 0, REG_SZ, (const BYTE*)cmd, (n + 1) * sizeof(wchar_t));
    } else {
        RegDeleteValueW(k, kRunName);
    }
    RegCloseKey(k);
}

static int Run(HINSTANCE hi)
{
    // Pikselleri gercek (olceklenmemis) boyutta yakala
    SetProcessDPIAware();

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const wchar_t* a = (argv && argc > 1) ? argv[1] : L"";
    while (*a == L'-' || *a == L'/') a++;                 // -f, --f, /f hepsi olur

    if (!lstrcmpiW(a, L"h") || !lstrcmpiW(a, L"help") || !lstrcmpiW(a, L"?")) {
        MessageBoxW(NULL,
            L"shot            arkaplanda başlat\n"
            L"shot -startup   Windows ile başlat\n"
            L"shot -nostartup otomatik başlatmayı kaldır\n"
            L"shot -exit      kapat\n"
            L"shot -f         tam ekran   [PrintScreen]\n"
            L"shot -w         aktif pencere   [Shift+PrintScreen]\n"
            L"shot -h|-help   yardım",
            L"shot", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (!lstrcmpiW(a, L"f"))         { Capture(false); return 0; }
    if (!lstrcmpiW(a, L"w"))         { Capture(true);  return 0; }
    if (!lstrcmpiW(a, L"exit"))      { HWND h = FindRunning(); if (h) PostMessageW(h, WM_CLOSE, 0, 0); return 0; }
    if (!lstrcmpiW(a, L"startup"))   { SetStartup(true);  StartDaemonProcess(); return 0; }
    if (!lstrcmpiW(a, L"nostartup")) { SetStartup(false); return 0; }
    if (*a == 0)                     { return RunDaemon(hi); }
    return 0;                                             // bilinmeyen bayrak: sessizce cik
}

extern "C" void entry()
{
    ExitProcess((UINT)Run(GetModuleHandleW(NULL)));
}
