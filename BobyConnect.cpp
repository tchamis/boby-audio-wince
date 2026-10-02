#include <windows.h>
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WM_APP
#define WM_APP 0x8000
#endif

#ifndef BOBY_PROFILE_PAN
#define BOBY_PROFILE_PAN 14
#endif
#ifndef BOBY_PROFILE_A2DP
#define BOBY_PROFILE_A2DP 1
#endif
#ifndef BOBY_PROFILE_AVRCP
#define BOBY_PROFILE_AVRCP 2
#endif

#define BOBY_PORT 47880
#define WM_BOBY_UPDATE (WM_APP + 23)
#define IDC_CONNECT 1001
#define IDC_PLAY    1002
#define IDC_PAUSE   1003
#define IDC_VWMENU  1004

typedef int  (*GetPairedDeviceTotalFn)();
typedef int  (*EstablishSLCPairedDeviceAtFn)(int, int);
typedef void (*ServiceSearchFn)(int);
typedef int  (*AVRCPCmdFn)(int, int, int);

static HWND g_hwnd = 0;
static SOCKET g_listenSocket = INVALID_SOCKET;
static volatile LONG g_bridgeConnected = 0;
static volatile LONG g_btBusy = 0;
static HMODULE g_nfbt = 0;
static GetPairedDeviceTotalFn g_getPairedTotal = 0;
static EstablishSLCPairedDeviceAtFn g_establish = 0;
static ServiceSearchFn g_serviceSearch = 0;
static AVRCPCmdFn g_avrcp = 0;

static wchar_t g_btStatus[160] = L"Bluetooth wird vorbereitet...";
static wchar_t g_bridgeStatus[160] = L"Boby Maps Bridge: warte auf Verbindung";
static wchar_t g_road[180] = L"Noch keine Navigation";
static wchar_t g_distance[80] = L"";
static wchar_t g_eta[80] = L"";
static wchar_t g_maneuver[80] = L"";
static int g_deviceIndex = 0;

static HFONT g_fontSmall = 0;
static HFONT g_fontMedium = 0;
static HFONT g_fontBig = 0;

static const wchar_t* kLogPath = L"\\My Flash Disk\\BobyConnect\\bobyconnect.log";

static void LogA(const char* text)
{
    HANDLE h = CreateFileW(kLogPath, GENERIC_WRITE, FILE_SHARE_READ, 0,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, 0, FILE_END);
    DWORD len = (DWORD)strlen(text);
    DWORD written = 0;
    WriteFile(h, text, len, &written, 0);
    WriteFile(h, "\r\n", 2, &written, 0);
    CloseHandle(h);
}

static void LogInts(const char* prefix, int a, int b, int c)
{
    char line[220];
    sprintf(line, "%s %d %d %d", prefix, a, b, c);
    LogA(line);
}

static void CopyWide(wchar_t* dst, int count, const wchar_t* src)
{
    if (!dst || count <= 0) return;
    if (!src) src = L"";
    int i = 0;
    for (; i < count - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static void Utf8ToWide(const char* src, wchar_t* dst, int count)
{
    if (!src || !dst || count <= 0) return;
    dst[0] = 0;
    int n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, count);
    if (n <= 0) {
        int i = 0;
        while (src[i] && i < count - 1) {
            unsigned char c = (unsigned char)src[i];
            dst[i] = (wchar_t)c;
            ++i;
        }
        dst[i] = 0;
    }
}

static void RequestRepaint()
{
    if (g_hwnd) PostMessageW(g_hwnd, WM_BOBY_UPDATE, 0, 0);
}

static void SetBtStatus(const wchar_t* s)
{
    CopyWide(g_btStatus, 160, s);
    RequestRepaint();
}

static void SetBridgeStatus(const wchar_t* s)
{
    CopyWide(g_bridgeStatus, 160, s);
    RequestRepaint();
}

static BOOL LoadBluetoothApi()
{
    if (g_nfbt) return TRUE;

    g_nfbt = LoadLibraryW(L"\\My Flash Disk\\BT\\nfBT.dll");
    if (!g_nfbt) {
        SetBtStatus(L"Bluetooth-API nfBT.dll nicht geladen");
        LogA("ERROR LoadLibrary nfBT.dll");
        return FALSE;
    }

    g_getPairedTotal = (GetPairedDeviceTotalFn)GetProcAddress(g_nfbt, L"GetPairedDeviceTotal");
    g_establish = (EstablishSLCPairedDeviceAtFn)GetProcAddress(g_nfbt, L"EstablishSLCPairedDeviceAt");
    g_serviceSearch = (ServiceSearchFn)GetProcAddress(g_nfbt, L"ServiceSearch");
    g_avrcp = (AVRCPCmdFn)GetProcAddress(g_nfbt, L"AVRCPCmd");

    if (!g_getPairedTotal || !g_establish) {
        SetBtStatus(L"Bluetooth-API unvollstaendig");
        LogA("ERROR required nfBT exports missing");
        return FALSE;
    }

    LogA("nfBT API loaded");
    return TRUE;
}

static void DoBluetoothConnect()
{
    if (InterlockedExchange(&g_btBusy, 1) != 0) return;

    if (!LoadBluetoothApi()) {
        InterlockedExchange(&g_btBusy, 0);
        return;
    }

    int total = g_getPairedTotal();
    if (total <= 0) {
        SetBtStatus(L"Kein gekoppeltes Telefon gefunden");
        LogInts("paired_total", total, 0, 0);
        InterlockedExchange(&g_btBusy, 0);
        return;
    }

    g_deviceIndex = 0;
    SetBtStatus(L"iPhone: PAN + Audio werden verbunden...");
    LogInts("paired_total/device", total, g_deviceIndex, 0);

    if (g_serviceSearch) {
        g_serviceSearch(g_deviceIndex);
        Sleep(500);
    }

    int pan = g_establish(g_deviceIndex, BOBY_PROFILE_PAN);
    Sleep(500);
    int a2dp = g_establish(g_deviceIndex, BOBY_PROFILE_A2DP);
    Sleep(350);
    int avrcp = g_establish(g_deviceIndex, BOBY_PROFILE_AVRCP);

    LogInts("Establish PAN/A2DP/AVRCP", pan, a2dp, avrcp);

    wchar_t status[160];
#ifdef BOBY_MAPPING_B
    wsprintfW(status, L"BT Mapping B | PAN:%d Audio:%d AVRCP:%d", pan, a2dp, avrcp);
#else
    wsprintfW(status, L"BT Mapping A | PAN:%d Audio:%d AVRCP:%d", pan, a2dp, avrcp);
#endif
    SetBtStatus(status);

    InterlockedExchange(&g_btBusy, 0);
}

static DWORD WINAPI BtConnectThread(LPVOID)
{
    DoBluetoothConnect();
    return 0;
}

static DWORD WINAPI AutoBtThread(LPVOID)
{
    Sleep(3000);
    DoBluetoothConnect();
    return 0;
}

static void StartBtConnect()
{
    HANDLE h = CreateThread(0, 0, BtConnectThread, 0, 0, 0);
    if (h) CloseHandle(h);
}

static void SendAvrcp(int command)
{
    if (!LoadBluetoothApi() || !g_avrcp) return;

    // nFore uses the paired device id plus the AVRCP operation code.
    // The third parameter is kept at zero as in the existing Boby build.
    int r = g_avrcp(g_deviceIndex, command, 0);
    char line[100];
    sprintf(line, "AVRCP cmd=0x%02X ret=%d", command, r);
    LogA(line);
}

static char* NextField(char** cursor)
{
    if (!cursor || !*cursor) return 0;
    char* start = *cursor;
    char* p = strchr(start, '|');
    if (p) {
        *p = 0;
        *cursor = p + 1;
    } else {
        *cursor = 0;
    }
    return start;
}

static void HandleProtocolLine(char* line)
{
    if (!line || !*line) return;

    if (strncmp(line, "BGM1|HELLO|", 11) == 0) {
        SetBridgeStatus(L"Boby Maps Bridge: VERBUNDEN");
        LogA("Bridge HELLO received");
        return;
    }

    if (strcmp(line, "BGM1|END") == 0) {
        CopyWide(g_road, 180, L"Navigation beendet");
        g_distance[0] = 0;
        g_eta[0] = 0;
        g_maneuver[0] = 0;
        RequestRepaint();
        return;
    }

    if (strncmp(line, "BGM1|NAV|", 9) != 0) return;

    char* cursor = line;
    char* magic = NextField(&cursor);
    char* type = NextField(&cursor);
    char* state = NextField(&cursor);
    char* maneuver = NextField(&cursor);
    char* distStep = NextField(&cursor);
    char* road = NextField(&cursor);
    char* etaSec = NextField(&cursor);
    char* distDest = NextField(&cursor);
    char* roundExit = NextField(&cursor);
    char* exitNumber = NextField(&cursor);

    (void)magic; (void)type; (void)state; (void)roundExit; (void)exitNumber;

    int man = maneuver ? atoi(maneuver) : 0;
    int stepM = distStep ? atoi(distStep) : 0;
    int eta = etaSec ? atoi(etaSec) : 0;
    int destM = distDest ? atoi(distDest) : 0;

    if (road) Utf8ToWide(road, g_road, 180);
    else CopyWide(g_road, 180, L"");

    if (stepM >= 1000)
        wsprintfW(g_distance, L"Naechstes Manoever: %d.%01d km", stepM / 1000, (stepM % 1000) / 100);
    else
        wsprintfW(g_distance, L"Naechstes Manoever: %d m", stepM);

    wsprintfW(g_eta, L"Rest: %d.%01d km  |  %d min",
              destM / 1000, (destM % 1000) / 100, eta / 60);

    wsprintfW(g_maneuver, L"Google Manoever %d", man);
    RequestRepaint();
}

static DWORD WINAPI NetworkThread(LPVOID)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        SetBridgeStatus(L"TCP konnte nicht gestartet werden");
        LogA("WSAStartup failed");
        return 0;
    }

    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET) {
        SetBridgeStatus(L"TCP Socket Fehler");
        LogA("socket failed");
        WSACleanup();
        return 0;
    }

    BOOL reuse = TRUE;
    setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(BOBY_PORT);

    if (bind(g_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        SetBridgeStatus(L"TCP Port 47880 konnte nicht geoeffnet werden");
        LogA("bind 47880 failed");
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return 0;
    }

    if (listen(g_listenSocket, 2) == SOCKET_ERROR) {
        SetBridgeStatus(L"TCP Listen Fehler");
        LogA("listen failed");
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return 0;
    }

    SetBridgeStatus(L"Boby Maps Bridge: Port 47880 bereit");
    LogA("TCP 47880 listening");

    for (;;) {
        SOCKET client = accept(g_listenSocket, 0, 0);
        if (client == INVALID_SOCKET) {
            Sleep(500);
            continue;
        }

        InterlockedExchange(&g_bridgeConnected, 1);
        SetBridgeStatus(L"Boby Maps Bridge: VERBUNDEN");
        LogA("TCP client connected");

        char accumulator[2048];
        int used = 0;
        ZeroMemory(accumulator, sizeof(accumulator));

        for (;;) {
            char buf[512];
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0) break;

            for (int i = 0; i < n; ++i) {
                char ch = buf[i];
                if (ch == '\r') continue;

                if (ch == '\n') {
                    accumulator[used] = 0;
                    HandleProtocolLine(accumulator);
                    used = 0;
                } else if (used < (int)sizeof(accumulator) - 1) {
                    accumulator[used++] = ch;
                } else {
                    used = 0;
                }
            }
        }

        closesocket(client);
        InterlockedExchange(&g_bridgeConnected, 0);
        SetBridgeStatus(L"Boby Maps Bridge getrennt - warte auf 47880");
        LogA("TCP client disconnected");
    }

    return 0;
}

static void DrawTextSimple(HDC dc, const wchar_t* text, RECT rc, HFONT font, UINT format)
{
    HFONT old = 0;
    if (font) old = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text, -1, &rc, format);
    if (old) SelectObject(dc, old);
}

static void PaintMain(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);

    RECT client;
    GetClientRect(hwnd, &client);

    HBRUSH bg = CreateSolidBrush(RGB(20, 24, 28));
    FillRect(dc, &client, bg);
    DeleteObject(bg);

    SetTextColor(dc, RGB(245,245,245));
    RECT r = {12, 8, 468, 38};
    DrawTextSimple(dc, L"BOBY CONNECT 0.3", r, g_fontMedium, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(dc, RGB(120,210,255));
    r.top = 38; r.bottom = 62;
    DrawTextSimple(dc, g_bridgeStatus, r, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(dc, RGB(190,190,190));
    r.top = 62; r.bottom = 84;
    DrawTextSimple(dc, g_btStatus, r, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(dc, RGB(255,255,255));
    r.left = 16; r.right = 464; r.top = 90; r.bottom = 130;
    DrawTextSimple(dc, g_maneuver[0] ? g_maneuver : L"MAPS", r, g_fontBig,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    r.top = 132; r.bottom = 166;
    DrawTextSimple(dc, g_distance[0] ? g_distance : L"Warte auf Boby Maps Bridge", r,
                   g_fontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(dc, RGB(220,220,220));
    r.top = 166; r.bottom = 194;
    DrawTextSimple(dc, g_road, r, g_fontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(dc, RGB(170,170,170));
    r.top = 194; r.bottom = 216;
    DrawTextSimple(dc, g_eta, r, g_fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    EndPaint(hwnd, &ps);
}

static void ShowVwMenu()
{
    HWND shell = FindWindowW(L"VwUserShell", L"VwUserShell");
    if (!shell) shell = FindWindowW(0, L"VwUserShell");

    if (shell) {
        ShowWindow(shell, SW_SHOW);
        BringWindowToTop(shell);
        SetForegroundWindow(shell);
    }

    ShowWindow(g_hwnd, SW_HIDE);
    LogA("VW MENU: Boby hidden");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CONNECT:
            SetBtStatus(L"PAN + A2DP werden angefordert...");
            StartBtConnect();
            return 0;
        case IDC_PLAY:
            SendAvrcp(0x44);
            return 0;
        case IDC_PAUSE:
            SendAvrcp(0x46);
            return 0;
        case IDC_VWMENU:
            ShowVwMenu();
            return 0;
        }
        break;

    case WM_BOBY_UPDATE:
        InvalidateRect(hwnd, 0, TRUE);
        UpdateWindow(hwnd);
        return 0;

    case WM_PAINT:
        PaintMain(hwnd);
        return 0;

    case WM_CLOSE:
        ShowVwMenu();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void BringExistingToFront(HWND existing)
{
    ShowWindow(existing, SW_SHOW);
    SetWindowPos(existing, HWND_TOP, 0, 0, 480, 272, SWP_SHOWWINDOW);
    BringWindowToTop(existing);
    SetForegroundWindow(existing);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    HWND existing = FindWindowW(L"BobyConnect", L"BOBY CONNECT");
    if (existing) {
        BringExistingToFront(existing);
        return 0;
    }

    LogA("===== BobyConnect 0.3 START =====");

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"BobyConnect";

    if (!RegisterClassW(&wc)) {
        LogA("RegisterClass failed");
        return 10;
    }

    g_hwnd = CreateWindowW(L"BobyConnect", L"BOBY CONNECT",
                           WS_POPUP | WS_VISIBLE,
                           0, 0, 480, 272,
                           0, 0, hInst, 0);

    if (!g_hwnd) {
        LogA("CreateWindow failed");
        return 11;
    }

    g_fontSmall = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    g_fontMedium = CreateFontW(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    g_fontBig = CreateFontW(27, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");

    HWND b1 = CreateWindowW(L"BUTTON", L"PAN + AUDIO", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            8, 224, 132, 40, g_hwnd, (HMENU)IDC_CONNECT, hInst, 0);
    HWND b2 = CreateWindowW(L"BUTTON", L"PLAY", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            146, 224, 72, 40, g_hwnd, (HMENU)IDC_PLAY, hInst, 0);
    HWND b3 = CreateWindowW(L"BUTTON", L"PAUSE", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            224, 224, 78, 40, g_hwnd, (HMENU)IDC_PAUSE, hInst, 0);
    HWND b4 = CreateWindowW(L"BUTTON", L"VW MENU", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            308, 224, 164, 40, g_hwnd, (HMENU)IDC_VWMENU, hInst, 0);

    if (g_fontSmall) {
        SendMessageW(b1, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
        SendMessageW(b2, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
        SendMessageW(b3, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
        SendMessageW(b4, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
    }

    BringExistingToFront(g_hwnd);

    HANDLE netThread = CreateThread(0, 0, NetworkThread, 0, 0, 0);
    if (netThread) CloseHandle(netThread);

    HANDLE btThread = CreateThread(0, 0, AutoBtThread, 0, 0, 0);
    if (btThread) CloseHandle(btThread);

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_nfbt) FreeLibrary(g_nfbt);
    if (g_fontSmall) DeleteObject(g_fontSmall);
    if (g_fontMedium) DeleteObject(g_fontMedium);
    if (g_fontBig) DeleteObject(g_fontBig);
    LogA("===== BobyConnect END =====");
    return 0;
}
