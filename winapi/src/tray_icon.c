/*
 * MIT License
 *
 * Copyright (c) 2018-2025 Vitaly Novichkov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <windows.h>

#include "misc.h"
#include "tray_icon.h"
#include "shot_data.h"
#include "shot_proc.h"
#include "settings.h"
#include "resource.h"
#include "resource_ex.h"


static HINSTANCE                lib_sh32 = NULL;
typedef BOOL (WINAPI *PtrShell_NotifyIcon)(DWORD,PNOTIFYICONDATA);
static PtrShell_NotifyIcon      ptrShell_NotifyIcon = NULL;

static uint32_t                 MYWM_TASKBARCREATED = 0;

HWND                            g_trayIconHWnd = NULL;
TrayIcon                        g_trayIcon;


void initLibraries()
{
    static int triedLoad = 0;

    if (!triedLoad)
    {
        if(!lib_sh32)
            lib_sh32 = LoadLibraryA("shell32");

        triedLoad = 1;
        ptrShell_NotifyIcon = (PtrShell_NotifyIcon)GetProcAddress(lib_sh32, "Shell_NotifyIconW");

        /* For restoring the tray icon after explorer crashes */
        if (!MYWM_TASKBARCREATED)
            MYWM_TASKBARCREATED = RegisterWindowMessageA("SysTrayIconClass");
    }
}

int detectShellVersion()
{
    DLLVERSIONINFO dvi;
    HRESULT hr;
    int shellVersion = 4;

    if(!lib_sh32)
        lib_sh32 = LoadLibraryA("shell32");

    DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(lib_sh32, "DllGetVersion");

    if(pDllGetVersion)
    {
        ZeroMemory(&dvi, sizeof(dvi));
        dvi.cbSize = sizeof(dvi);
        hr = (*pDllGetVersion)(&dvi);

        if (SUCCEEDED(hr) && dvi.dwMajorVersion >= 5)
            shellVersion = dvi.dwMajorVersion;
    }

    return shellVersion;
}



static void ShowPopupMenu(HWND hWnd)
{
    HMENU menu = CreatePopupMenu();
    POINT pt;

    if(menu)
    {
        InsertMenuA(menu, -1, MF_BYPOSITION, IDM_SETTINGS, "Settings");
        InsertMenuA(menu, -1, MF_BYPOSITION, IDM_SAVECLIP, "Save image in clipboard");
        InsertMenuA(menu, -1, MF_SEPARATOR, -1, "");
        InsertMenuA(menu, -1, MF_BYPOSITION, IDM_QUIT, "Quit");

        SetForegroundWindow(hWnd);

        GetCursorPos(&pt);
        TrackPopupMenu(menu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(menu);
    }
}

static BOOL OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    BOOL ret = TRUE;
    /* int wNotifyCode = HIWORD(wParam);   // Notification code. */
    int wID = LOWORD(wParam);   // Item, control, or accelerator identifier.
    /* HWND hwndCtl    = (HWND)lParam;     // Handle of control. */

    (VOID)lParam;

    switch(wID)
    {
    case IDM_SETTINGS:
        settingsOpen();
        break;

    case IDM_SAVECLIP:
        cmd_dumpClipboard(hWnd, &g_shotData);
        break;

    case IDM_QUIT:
        if(IsWindowVisible(hWnd))
            SendMessage(hWnd, WM_DESTROY, (WPARAM)0, (LPARAM)0);
        else
            SendMessage(hWnd, WM_CLOSE, (WPARAM)0, (LPARAM)0);
        settingsDestroy();
        closeSysTrayIcon();
        shotProc_quit();
        break;

    case ID_CMD_MAKE_SHOT:
        cmd_makeScreenshot(hWnd, &g_shotData);
        break;

    default:
        ret = FALSE;
    }

    return ret;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case MYWM_NOTIFYICON:
        switch(lParam)
        {
        case WM_LBUTTONDBLCLK:
            settingsOpen();
            break;
        case WM_RBUTTONDOWN:
            ShowPopupMenu(hWnd);
            break;
        }

        break;

    case WM_COMMAND:
        return OnCommand(hWnd, wParam, lParam);

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

void sysTraySetIcon(IconToSet icon)
{
    if(!g_trayIconHWnd)
    {
        debugLog("-- Attempt to switch icon failed\n");
        return;
    }

    switch(icon)
    {
    case SET_ICON_NORMAL:
        if(g_trayIcon.tnd.hIcon == g_trayIcon.hIcon16)
            return;
        g_trayIcon.tnd.hIcon = g_trayIcon.hIcon16;
        debugLog("-- Toggle icon to Normal\n");
        break;

    case SET_ICON_BUSY:
        if(g_trayIcon.tnd.hIcon == g_trayIcon.hIconBusy)
            return;
        g_trayIcon.tnd.hIcon = g_trayIcon.hIconBusy;
        debugLog("-- Toggle icon to Busy\n");
        break;

    case SET_ICON_UPLOAD:
        if(g_trayIcon.tnd.hIcon == g_trayIcon.hIconUpload)
            return;
        g_trayIcon.tnd.hIcon = g_trayIcon.hIconUpload;
        debugLog("-- Toggle icon to Upload\n");
        break;

    default:
        return;
    }

    Shell_NotifyIconA(NIM_MODIFY, &g_trayIcon.tnd);
}

ATOM regMyWindowClass(HINSTANCE hInst, LPCSTR lpzClassName)
{
    WNDCLASS wcWindowClass;
    ZeroMemory(&wcWindowClass, sizeof(WNDCLASS));
    wcWindowClass.lpfnWndProc = (WNDPROC)WndProc;
    wcWindowClass.style = CS_HREDRAW|CS_VREDRAW;
    wcWindowClass.hInstance = hInst;
    wcWindowClass.lpszClassName = lpzClassName;
    wcWindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wcWindowClass.hbrBackground = (HBRUSH)COLOR_APPWORKSPACE;

    return RegisterClassA(&wcWindowClass);
}

int initSysTrayIcon(HINSTANCE hInstance)
{
    LPCSTR lpzClass = "TinyShotTrayIconClass";

    ZeroMemory(&g_trayIcon, sizeof(g_trayIcon));
    initLibraries();

    g_trayIcon.currentShellVersion = detectShellVersion();
    g_trayIcon.notifyIconSizeA = FIELD_OFFSET(NOTIFYICONDATAA, szTip[64]);
    g_trayIcon.notifyIconSizeW = FIELD_OFFSET(NOTIFYICONDATAW, szTip[64]);
    g_trayIcon.maxTipLength = 64;

    if(!regMyWindowClass(hInstance, lpzClass))
        return 1;

    g_trayIcon.hIcon32 = (HICON)LoadImageA(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    g_trayIcon.hIcon16 = (HICON)LoadImageA(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    g_trayIcon.hIconBusy = (HICON)LoadImageA(hInstance, MAKEINTRESOURCE(IDI_ICON_BUSY), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    g_trayIcon.hIconUpload = (HICON)LoadImageA(hInstance, MAKEINTRESOURCE(IDI_ICON_UPLOAD), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    g_trayIconHWnd = CreateWindowA(lpzClass, "SysTrayWindow", WS_OVERLAPPEDWINDOW,
                                   100, 100, 300, 150, NULL, NULL, hInstance, NULL);

    if(!g_trayIconHWnd)
        return 2;

    SendMessage(g_trayIconHWnd, WM_SETICON, ICON_BIG, (LPARAM)g_trayIcon.hIcon32);
    SendMessage(g_trayIconHWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_trayIcon.hIcon16);

    memset(&g_trayIcon.tnd, 0, g_trayIcon.notifyIconSizeA);
    g_trayIcon.tnd.uID = 0;
    g_trayIcon.tnd.cbSize = g_trayIcon.notifyIconSizeA;
    g_trayIcon.tnd.hWnd = g_trayIconHWnd;
    g_trayIcon.tnd.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
    g_trayIcon.tnd.uCallbackMessage = MYWM_NOTIFYICON;
    g_trayIcon.tnd.hIcon = g_trayIcon.hIcon16;

    Shell_NotifyIconA(NIM_ADD, &g_trayIcon.tnd);

    return 0;
}

void closeSysTrayIcon()
{
    if(g_trayIcon.hIcon32)
        DestroyIcon(g_trayIcon.hIcon32);

    if(g_trayIcon.hIcon16)
        DestroyIcon(g_trayIcon.hIcon16);

    if(g_trayIcon.hIconBusy)
        DestroyIcon(g_trayIcon.hIconBusy);

    if(g_trayIcon.hIconUpload)
        DestroyIcon(g_trayIcon.hIconUpload);

    Shell_NotifyIconA(NIM_DELETE, &g_trayIcon.tnd);
    if(lib_sh32)
    {
        FreeLibrary(lib_sh32);
        lib_sh32 = NULL;
    }
}
