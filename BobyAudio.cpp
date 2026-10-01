#include <windows.h>
#include <wchar.h>

typedef bool (*StartApplicationFn)(const wchar_t*, const wchar_t*, const bool&);
typedef bool (*GetForegroundApplicationFn)(wchar_t*, int);

static const wchar_t* kLogPath = L"\\My Flash Disk\\boby_audio.log";
static const wchar_t* kDllPath = L"\\My Flash Disk\\VwUserShell\\ACAppCom.dll";
static const wchar_t* kAppName = L"MMPMediaPlayer";
static const wchar_t* kExportStart = L"?StartApplication@AppCom@@YA_NPB_W0AB_N@Z";
static const wchar_t* kExportForeground = L"?GetForegroundApplication@AppCom@@YA_NPA_WH@Z";

static void Log(const char* text)
{
    HANDLE h = CreateFile(kLogPath, GENERIC_WRITE, FILE_SHARE_READ, 0,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        return;

    SetFilePointer(h, 0, 0, FILE_END);
    DWORD written = 0;
    DWORD len = 0;
    while (text[len] != 0) ++len;
    WriteFile(h, text, len, &written, 0);
    WriteFile(h, "\r\n", 2, &written, 0);
    CloseHandle(h);
}

static bool IsPlayerForeground(GetForegroundApplicationFn getForeground)
{
    if (!getForeground)
        return false;

    wchar_t name[64];
    ZeroMemory(name, sizeof(name));

    if (!getForeground(name, 64))
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

    Log("BobyAudio 0.1 starting");

    // autorunce starts us early. Wait until the original VW user shell exists.
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

    Log(shellSeen ? "VwUserShell detected" : "VwUserShell wait timed out");

    // Give Bluetooth/A2DP time to reconnect. We intentionally do not fake Play:
    // the original player starts playback itself after being opened.
    Sleep(12000);

    SetCurrentDirectory(L"\\My Flash Disk\\VwUserShell");

    HMODULE appCom = LoadLibrary(kDllPath);
    if (!appCom) {
        Log("ERROR: LoadLibrary ACAppCom.dll failed");
        if (singleton) CloseHandle(singleton);
        return 10;
    }

    StartApplicationFn startApplication =
        reinterpret_cast<StartApplicationFn>(GetProcAddress(appCom, kExportStart));

    GetForegroundApplicationFn getForeground =
        reinterpret_cast<GetForegroundApplicationFn>(GetProcAddress(appCom, kExportForeground));

    if (!startApplication) {
        Log("ERROR: StartApplication export not found");
        FreeLibrary(appCom);
        if (singleton) CloseHandle(singleton);
        return 11;
    }

    Log("AppCom exports resolved");

    // Retry for roughly one minute. This avoids racing the Bluetooth reconnect.
    const bool bringToForeground = true;
    bool success = false;

    for (int attempt = 0; attempt < 15; ++attempt) {
        bool result = startApplication(kAppName, L"", bringToForeground);

        if (result)
            Log("StartApplication returned TRUE");
        else
            Log("StartApplication returned FALSE");

        Sleep(1500);

        if (IsPlayerForeground(getForeground)) {
            Log("SUCCESS: MMPMediaPlayer is foreground");
            success = true;
            break;
        }

        Sleep(2500);
    }

    if (!success)
        Log("FAILED: player did not become foreground");

    FreeLibrary(appCom);
    if (singleton) CloseHandle(singleton);
    return success ? 0 : 20;
}
