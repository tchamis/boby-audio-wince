#include <windows.h>
#include <wchar.h>

typedef bool (*StartApplicationFn)(const wchar_t*, const wchar_t*, const bool&);
typedef bool (*IsApplicationRunningFn)(const wchar_t*);
typedef bool (*GetForegroundApplicationFn)(wchar_t*, int);
typedef bool (*GetStringForAppFn)(const wchar_t*, wchar_t**);

static const wchar_t* kLogPath = L"\\My Flash Disk\\boby_audio.log";
static const wchar_t* kDllPath = L"\\My Flash Disk\\VwUserShell\\ACAppCom.dll";
static const wchar_t* kAppName = L"MMPMediaPlayer";

static const wchar_t* kExportStart =
    L"?StartApplication@AppCom@@YA_NPB_W0AB_N@Z";
static const wchar_t* kExportRunning =
    L"?IsApplicationRunning@AppCom@@YA_NPB_W@Z";
static const wchar_t* kExportForeground =
    L"?GetForegroundApplication@AppCom@@YA_NPA_WH@Z";
static const wchar_t* kExportGetAppPath =
    L"?GetAppPath@AppCom@@YA_NPB_WPAPA_W@Z";
static const wchar_t* kExportGetExeParameters =
    L"?GetExeParameters@AppCom@@YA_NPB_WPAPA_W@Z";

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

static void LogWide(const char* prefix, const wchar_t* value)
{
    char line[512];
    int p = 0;

    while (prefix[p] && p < 400) {
        line[p] = prefix[p];
        ++p;
    }

    if (!value) {
        const char* nullText = "<null>";
        int i = 0;
        while (nullText[i] && p < 500)
            line[p++] = nullText[i++];
    } else {
        int i = 0;
        while (value[i] && p < 500) {
            wchar_t c = value[i++];
            line[p++] = (c >= 32 && c < 127) ? (char)c : '?';
        }
    }

    line[p] = 0;
    Log(line);
}

static void LogBool(const char* prefix, bool value)
{
    char line[160];
    int p = 0;

    while (prefix[p] && p < 140) {
        line[p] = prefix[p];
        ++p;
    }

    const char* v = value ? "TRUE" : "FALSE";
    int i = 0;
    while (v[i] && p < 150)
        line[p++] = v[i++];

    line[p] = 0;
    Log(line);
}

static void LogForeground(GetForegroundApplicationFn fn)
{
    if (!fn) {
        Log("GetForegroundApplication export missing");
        return;
    }

    wchar_t name[128];
    ZeroMemory(name, sizeof(name));

    bool ok = fn(name, 128);
    LogBool("GetForegroundApplication returned ", ok);
    if (ok)
        LogWide("Foreground = ", name);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    HANDLE singleton = CreateMutex(0, FALSE, L"BobyAudio.Singleton");
    if (singleton && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleton);
        return 0;
    }

    Log("===== BobyAudio 0.2 =====");
    Log("START");

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

    LogBool("VwUserShell detected = ", shellSeen);

    // Let the stock Bluetooth stack reconnect before probing AppCom.
    Sleep(12000);

    HMODULE appCom = LoadLibrary(kDllPath);
    if (!appCom) {
        Log("ERROR: LoadLibrary ACAppCom.dll failed");
        if (singleton) CloseHandle(singleton);
        return 10;
    }

    Log("ACAppCom.dll loaded");

    StartApplicationFn startApplication =
        reinterpret_cast<StartApplicationFn>(GetProcAddress(appCom, kExportStart));
    IsApplicationRunningFn isApplicationRunning =
        reinterpret_cast<IsApplicationRunningFn>(GetProcAddress(appCom, kExportRunning));
    GetForegroundApplicationFn getForeground =
        reinterpret_cast<GetForegroundApplicationFn>(GetProcAddress(appCom, kExportForeground));
    GetStringForAppFn getAppPath =
        reinterpret_cast<GetStringForAppFn>(GetProcAddress(appCom, kExportGetAppPath));
    GetStringForAppFn getExeParameters =
        reinterpret_cast<GetStringForAppFn>(GetProcAddress(appCom, kExportGetExeParameters));

    LogBool("StartApplication export = ", startApplication != 0);
    LogBool("IsApplicationRunning export = ", isApplicationRunning != 0);
    LogBool("GetForegroundApplication export = ", getForeground != 0);
    LogBool("GetAppPath export = ", getAppPath != 0);
    LogBool("GetExeParameters export = ", getExeParameters != 0);

    if (!startApplication) {
        FreeLibrary(appCom);
        if (singleton) CloseHandle(singleton);
        return 11;
    }

    if (isApplicationRunning) {
        LogBool("Running VwUserShell = ", isApplicationRunning(L"VwUserShell"));
        LogBool("Running Bluetooth = ", isApplicationRunning(L"Bluetooth"));
        LogBool("Running MMPMediaPlayer = ", isApplicationRunning(kAppName));
    }

    if (getAppPath) {
        wchar_t* path = 0;
        bool ok = getAppPath(kAppName, &path);
        LogBool("GetAppPath(MMPMediaPlayer) = ", ok);
        if (ok)
            LogWide("AppPath = ", path);
    }

    if (getExeParameters) {
        wchar_t* params = 0;
        bool ok = getExeParameters(kAppName, &params);
        LogBool("GetExeParameters(MMPMediaPlayer) = ", ok);
        if (ok)
            LogWide("ExeParameters = ", params);
    }

    LogForeground(getForeground);

    // Diagnostic: test both observed possibilities for the const bool& argument.
    bool flagFalse = false;
    bool resultFalse = startApplication(kAppName, L"", flagFalse);
    LogBool("StartApplication(flag=FALSE) = ", resultFalse);
    Sleep(2500);

    if (isApplicationRunning)
        LogBool("Running MMPMediaPlayer after FALSE = ",
                isApplicationRunning(kAppName));
    LogForeground(getForeground);

    bool flagTrue = true;
    bool resultTrue = startApplication(kAppName, L"", flagTrue);
    LogBool("StartApplication(flag=TRUE) = ", resultTrue);
    Sleep(2500);

    if (isApplicationRunning)
        LogBool("Running MMPMediaPlayer after TRUE = ",
                isApplicationRunning(kAppName));
    LogForeground(getForeground);

    Log("END");

    FreeLibrary(appCom);
    if (singleton) CloseHandle(singleton);
    return 0;
}
