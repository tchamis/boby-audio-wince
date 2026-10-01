#include <windows.h>
#include <wchar.h>

typedef bool (*StartApplicationFn)(const wchar_t*, const wchar_t*, const bool&);
typedef bool (*IsApplicationRunningFn)(const wchar_t*);
typedef bool (*GetForegroundApplicationFn)(wchar_t*, int);

static const wchar_t* kLogPath = L"\\My Flash Disk\\boby_audio.log";
static const wchar_t* kDllPath = L"\\My Flash Disk\\VwUserShell\\ACAppCom.dll";
static const wchar_t* kAppName = L"MMPMediaPlayer";

static const wchar_t* kExportStart =
    L"?StartApplication@AppCom@@YA_NPB_W0AB_N@Z";
static const wchar_t* kExportRunning =
    L"?IsApplicationRunning@AppCom@@YA_NPB_W@Z";
static const wchar_t* kExportForeground =
    L"?GetForegroundApplication@AppCom@@YA_NPA_WH@Z";

static void Log(const char* text)
{
    HANDLE h = CreateFile(kLogPath, GENERIC_WRITE, FILE_SHARE_READ, 0,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        return;

    SetFilePointer(h, 0, 0, FILE_END);

    DWORD len = 0;
    while (text[len] != 0)
        ++len;

    DWORD written = 0;
    WriteFile(h, text, len, &written, 0);
    WriteFile(h, "\r\n", 2, &written, 0);
    CloseHandle(h);
}

static void LogBool(const char* prefix, bool value)
{
    char line[160];
    int p = 0;

    while (prefix[p] && p < 140)
        line[p] = prefix[p], ++p;

    const char* v = value ? "TRUE" : "FALSE";
    int i = 0;
    while (v[i] && p < 150)
        line[p++] = v[i++];

    line[p] = 0;
    Log(line);
}

static bool IsPlayerForeground(GetForegroundApplicationFn fn)
{
    if (!fn)
        return false;

    wchar_t name[128];
    ZeroMemory(name, sizeof(name));

    if (!fn(name, 128))
        return false;

    return wcscmp(name, kAppName) == 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    HANDLE singleton = CreateMutex(0, FALSE, L"BobyAudio.Singleton");
    if (singleton && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleton);
        return 0;
    }

    Log("===== BobyAudio 0.3 =====");
    Log("START");

    // Wait until the original shell window exists.
    bool shellSeen = false;
    for (int i = 0; i < 60; ++i) {
        HWND shell = FindWindow(L"VwUserShell", L"VwUserShell");
        if (!shell)
            shell = FindWindow(0, L"VwUserShell");

        if (shell) {
            shellSeen = true;
            break;
        }
        Sleep(1000);
    }

    LogBool("VwUserShell window detected = ", shellSeen);

    HMODULE appCom = LoadLibrary(kDllPath);
    if (!appCom) {
        Log("ERROR: LoadLibrary ACAppCom.dll failed");
        if (singleton) CloseHandle(singleton);
        return 10;
    }

    StartApplicationFn startApplication =
        reinterpret_cast<StartApplicationFn>(GetProcAddress(appCom, kExportStart));
    IsApplicationRunningFn isApplicationRunning =
        reinterpret_cast<IsApplicationRunningFn>(GetProcAddress(appCom, kExportRunning));
    GetForegroundApplicationFn getForeground =
        reinterpret_cast<GetForegroundApplicationFn>(GetProcAddress(appCom, kExportForeground));

    if (!startApplication || !isApplicationRunning) {
        Log("ERROR: required AppCom export missing");
        FreeLibrary(appCom);
        if (singleton) CloseHandle(singleton);
        return 11;
    }

    // Important: this EXE must live in \My Flash Disk\VwUserShell.
    // AppCom resolves NgAppCom.xml relative to the process/module context.
    LogBool("AppCom sees VwUserShell = ", isApplicationRunning(L"VwUserShell"));

    // Give the stock stack a moment, then wait until the Bluetooth application
    // is registered/running. The retry loop below still handles late phone/A2DP reconnects.
    Sleep(8000);

    bool btSeen = false;
    for (int i = 0; i < 25; ++i) {
        if (isApplicationRunning(L"Bluetooth")) {
            btSeen = true;
            break;
        }
        Sleep(1000);
    }
    LogBool("Bluetooth running = ", btSeen);

    const bool bringToForeground = true;
    bool success = false;

    // Ask the original AppCom layer to launch/show the stock player.
    // Do not emulate touch and do not send AVRCP Play: the stock player
    // starts playback by itself after being opened.
    for (int attempt = 0; attempt < 15; ++attempt) {
        bool result = startApplication(kAppName, L"", bringToForeground);
        LogBool("StartApplication(TRUE) = ", result);

        Sleep(2000);

        bool running = isApplicationRunning(kAppName);
        LogBool("MMPMediaPlayer running = ", running);

        if (running && (!getForeground || IsPlayerForeground(getForeground))) {
            success = true;
            Log("SUCCESS: MMPMediaPlayer opened");
            break;
        }

        Sleep(2000);
    }

    if (!success)
        Log("FAILED: MMPMediaPlayer did not open");

    Log("END");

    FreeLibrary(appCom);
    if (singleton) CloseHandle(singleton);
    return success ? 0 : 20;
}
