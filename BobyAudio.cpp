#include <windows.h>
#include <wchar.h>

typedef bool (*StartApplicationFn)(const wchar_t*, const wchar_t*, const bool&);
typedef bool (*IsApplicationRunningFn)(const wchar_t*);

static const wchar_t* kLogPath = L"\\My Flash Disk\\boby_audio.log";
static const wchar_t* kDllPath = L"\\My Flash Disk\\VwUserShell\\ACAppCom.dll";
static const wchar_t* kAppName = L"MMPMediaPlayer";

static const wchar_t* kExportStart =
    L"?StartApplication@AppCom@@YA_NPB_W0AB_N@Z";
static const wchar_t* kExportRunning =
    L"?IsApplicationRunning@AppCom@@YA_NPB_W@Z";

static void Log(const char* text)
{
    HANDLE h = CreateFile(kLogPath, GENERIC_WRITE, FILE_SHARE_READ, 0,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, 0, FILE_END);
    DWORD len = 0; while (text[len] != 0) ++len;
    DWORD written = 0;
    WriteFile(h, text, len, &written, 0);
    WriteFile(h, "\r\n", 2, &written, 0);
    CloseHandle(h);
}

static void LogBool(const char* prefix, bool value)
{
    char line[180]; int p = 0;
    while (prefix[p] && p < 150) { line[p] = prefix[p]; ++p; }
    const char* v = value ? "TRUE" : "FALSE";
    int i = 0; while (v[i] && p < 170) line[p++] = v[i++];
    line[p] = 0; Log(line);
}

static void LogWide(const char* prefix, const wchar_t* value)
{
    char line[320]; int p = 0;
    while (prefix[p] && p < 220) { line[p] = prefix[p]; ++p; }

    if (!value) {
        const char* n = "<null>"; int i = 0;
        while (n[i] && p < 310) line[p++] = n[i++];
    } else {
        int i = 0;
        while (value[i] && p < 310) {
            wchar_t c = value[i++];
            line[p++] = (c >= 32 && c < 127) ? (char)c : '?';
        }
    }

    line[p] = 0;
    Log(line);
}

static void LogNativeForeground()
{
    HWND fg = GetForegroundWindow();
    LogBool("Win32 foreground exists = ", fg != 0);
    if (!fg) return;

    wchar_t cls[128]; wchar_t title[128];
    ZeroMemory(cls, sizeof(cls));
    ZeroMemory(title, sizeof(title));

    GetClassName(fg, cls, 128);
    GetWindowText(fg, title, 128);

    LogWide("Win32 foreground class = ", cls);
    LogWide("Win32 foreground title = ", title);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    HANDLE singleton = CreateMutex(0, FALSE, L"BobyAudio.Singleton");
    if (singleton && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleton);
        return 0;
    }

    Log("===== BobyAudio 0.5 =====");
    Log("START");

    for (int i = 0; i < 60; ++i) {
        HWND shell = FindWindow(L"VwUserShell", L"VwUserShell");
        if (!shell) shell = FindWindow(0, L"VwUserShell");
        if (shell) break;
        Sleep(1000);
    }

    HMODULE appCom = LoadLibrary(kDllPath);
    if (!appCom) {
        Log("ERROR: ACAppCom.dll load failed");
        if (singleton) CloseHandle(singleton);
        return 10;
    }

    StartApplicationFn startApplication =
        reinterpret_cast<StartApplicationFn>(GetProcAddress(appCom, kExportStart));
    IsApplicationRunningFn isRunning =
        reinterpret_cast<IsApplicationRunningFn>(GetProcAddress(appCom, kExportRunning));

    if (!startApplication || !isRunning) {
        Log("ERROR: required AppCom export missing");
        FreeLibrary(appCom);
        if (singleton) CloseHandle(singleton);
        return 11;
    }

    LogBool("AppCom sees VwUserShell = ", isRunning(L"VwUserShell"));

    for (int i = 0; i < 30 && !isRunning(L"Bluetooth"); ++i)
        Sleep(1000);
    LogBool("Bluetooth running = ", isRunning(L"Bluetooth"));

    Sleep(12000);

    const bool bringToForeground = true;
    LogBool("StartApplication(TRUE) = ",
            startApplication(kAppName, L"", bringToForeground));

    HWND player = 0;
    for (int i = 0; i < 15; ++i) {
        player = FindWindow(L"MMPMediaPlayer", 0);
        if (!player) player = FindWindow(0, L"MMPMediaPlayer");
        if (player) break;
        Sleep(1000);
    }

    LogBool("MMP native window found = ", player != 0);
    LogNativeForeground();

    if (player) {
        LogBool("MMP initially visible = ", IsWindowVisible(player) != FALSE);

        // AppCom reports MMPMediaPlayer as foreground on this device, while the
        // VW shell can still remain visually on top. Force the actual Win32
        // window to the front once it exists.
        ShowWindow(player, SW_SHOW);
        BringWindowToTop(player);
        SetWindowPos(player, HWND_TOP, 0, 0, 480, 272,
                     SWP_SHOWWINDOW);
        SetForegroundWindow(player);

        Sleep(1000);

        Log("After native foreground request:");
        LogBool("MMP visible = ", IsWindowVisible(player) != FALSE);
        LogNativeForeground();
    }

    // Keep the helper alive for 30 seconds so the DLL and state remain valid,
    // and re-assert foreground if the shell steals it immediately.
    for (int t = 0; t < 30; ++t) {
        if (player && IsWindow(player)) {
            HWND fg = GetForegroundWindow();
            if (fg != player) {
                ShowWindow(player, SW_SHOW);
                BringWindowToTop(player);
                SetWindowPos(player, HWND_TOP, 0, 0, 480, 272,
                             SWP_SHOWWINDOW);
                SetForegroundWindow(player);
                Log("Reasserted native MMP foreground");
            }
        }
        Sleep(1000);
    }

    Log("END");
    FreeLibrary(appCom);
    if (singleton) CloseHandle(singleton);
    return 0;
}
