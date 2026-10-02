#include <windows.h>

static void BringBoby(HWND boby)
{
    ShowWindow(boby, SW_SHOW);
    SetWindowPos(boby, HWND_TOP, 0, 0, 480, 272, SWP_SHOWWINDOW);
    BringWindowToTop(boby);
    SetForegroundWindow(boby);
}

static LRESULT CALLBACK DummyProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = DummyProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"ManualReaderClassNameNR";
    RegisterClassW(&wc);

    // Give the stock AppCom entry the window/class it expects for a few seconds.
    HWND dummy = CreateWindowW(L"ManualReaderClassNameNR", L"Manual_Reader",
                               WS_POPUP,
                               -20, -20, 1, 1,
                               0, 0, hInst, 0);

    HWND boby = FindWindowW(L"BobyConnect", L"BOBY CONNECT");
    if (!boby) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);

        if (CreateProcessW(L"\\My Flash Disk\\BobyConnect\\BobyConnect.exe",
                           0, 0, 0, FALSE, 0, 0, 0, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }

        for (int i = 0; i < 100; ++i) {
            boby = FindWindowW(L"BobyConnect", L"BOBY CONNECT");
            if (boby) break;
            Sleep(100);
        }
    }

    if (boby) BringBoby(boby);

    // Keep the expected ManualReader window around briefly so VwUserShell/AppCom
    // sees a successful launch, then leave BobyConnect running independently.
    Sleep(5000);
    if (dummy) DestroyWindow(dummy);
    return 0;
}
