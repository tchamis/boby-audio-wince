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

static void LogInt(const char* prefix, int value)
{
    char line[180]; int p=0;
    while (prefix[p] && p < 140) { line[p]=prefix[p]; ++p; }
    char tmp[32]; int n=0;
    if (value==0) tmp[n++]='0';
    else {
        if (value<0) { line[p++]='-'; value=-value; }
        char rev[32]; int r=0;
        while(value>0 && r<30){ rev[r++]=(char)('0'+(value%10)); value/=10; }
        while(r>0) tmp[n++]=rev[--r];
    }
    for(int i=0;i<n && p<170;++i) line[p++]=tmp[i];
    line[p]=0; Log(line);
}

static void LogWide(const char* prefix, const wchar_t* value)
{
    char line[300]; int p=0;
    while(prefix[p] && p<220){ line[p]=prefix[p]; ++p; }
    if (!value) {
        const char* n="<null>"; int i=0;
        while(n[i] && p<290) line[p++]=n[i++];
    } else {
        int i=0;
        while(value[i] && p<290){
            wchar_t c=value[i++];
            line[p++]=(c>=32 && c<127)?(char)c:'?';
        }
    }
    line[p]=0; Log(line);
}

static bool ForegroundName(GetForegroundApplicationFn fn, wchar_t* outName, int cch)
{
    if (!fn) return false;
    ZeroMemory(outName, sizeof(wchar_t)*cch);
    return fn(outName, cch);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    HANDLE singleton = CreateMutex(0, FALSE, L"BobyAudio.Singleton");
    if (singleton && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleton);
        return 0;
    }

    Log("===== BobyAudio 0.4 =====");
    Log("START");

    for (int i=0; i<60; ++i) {
        HWND shell=FindWindow(L"VwUserShell", L"VwUserShell");
        if (!shell) shell=FindWindow(0, L"VwUserShell");
        if (shell) break;
        Sleep(1000);
    }

    HMODULE appCom=LoadLibrary(kDllPath);
    if (!appCom) {
        Log("ERROR: ACAppCom.dll load failed");
        if (singleton) CloseHandle(singleton);
        return 10;
    }

    StartApplicationFn startApplication=
        reinterpret_cast<StartApplicationFn>(GetProcAddress(appCom,kExportStart));
    IsApplicationRunningFn isRunning=
        reinterpret_cast<IsApplicationRunningFn>(GetProcAddress(appCom,kExportRunning));
    GetForegroundApplicationFn getForeground=
        reinterpret_cast<GetForegroundApplicationFn>(GetProcAddress(appCom,kExportForeground));

    if (!startApplication || !isRunning || !getForeground) {
        Log("ERROR: required AppCom export missing");
        FreeLibrary(appCom);
        if (singleton) CloseHandle(singleton);
        return 11;
    }

    LogBool("AppCom sees VwUserShell = ", isRunning(L"VwUserShell"));

    // Wait for the Bluetooth application, but do not treat that as A2DP-ready.
    for (int i=0; i<30 && !isRunning(L"Bluetooth"); ++i)
        Sleep(1000);
    LogBool("Bluetooth running = ", isRunning(L"Bluetooth"));

    // Give the stock stack additional time to finish its reconnect.
    Sleep(12000);

    const bool bringToForeground=true;
    bool r=startApplication(kAppName, L"", bringToForeground);
    LogBool("StartApplication(TRUE) = ", r);

    // Keep this process and ACAppCom loaded and observe what actually happens
    // for 30 seconds instead of exiting as soon as AppCom reports success.
    for (int t=0; t<30; ++t) {
        bool running=isRunning(kAppName);
        LogBool("MMP running = ", running);

        wchar_t fg[128];
        bool fgOk=ForegroundName(getForeground, fg, 128);
        LogBool("Foreground query = ", fgOk);
        if (fgOk) LogWide("Foreground = ", fg);

        HWND byClass=FindWindow(L"MMPMediaPlayer", 0);
        HWND byTitle=FindWindow(0, L"MMPMediaPlayer");
        HWND player=byClass ? byClass : byTitle;

        LogBool("Native MMP window found = ", player != 0);
        if (player) {
            LogBool("Native MMP visible = ", IsWindowVisible(player) != FALSE);
            RECT rc; ZeroMemory(&rc,sizeof(rc));
            if (GetWindowRect(player,&rc)) {
                LogInt("MMP left = ", rc.left);
                LogInt("MMP top = ", rc.top);
                LogInt("MMP right = ", rc.right);
                LogInt("MMP bottom = ", rc.bottom);
            }
        }

        Sleep(1000);
    }

    Log("END");
    FreeLibrary(appCom);
    if (singleton) CloseHandle(singleton);
    return 0;
}
