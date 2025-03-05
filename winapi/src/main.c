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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <shlwapi.h>
#include "resource.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef BOOL (WINAPI *PtrShell_NotifyIcon)(DWORD,PNOTIFYICONDATA);
static PtrShell_NotifyIcon ptrShell_NotifyIcon = NULL;
static HINSTANCE lib_sh32 = NULL;

static uint32_t MYWM_TASKBARCREATED = 0;
#define MYWM_NOTIFYICON (WM_APP + 101)

HWND s_mainWindow = NULL;

void initLibraries()
{
    static int triedLoad = 0;

    if (!triedLoad)
    {
        if(!lib_sh32)
            lib_sh32 = LoadLibraryA("shell32");

        triedLoad = 1;
        ptrShell_NotifyIcon = (PtrShell_NotifyIcon)GetProcAddress(lib_sh32, "Shell_NotifyIconW");
    }
}

struct TrayIcon
{
    NOTIFYICONDATAA tnd;

    uint32_t notifyIconSizeW;
    uint32_t notifyIconSizeA;

    int currentShellVersion;
    int maxTipLength;
    HICON hIcon32;
    HICON hIcon16;
};

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

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
ATOM RegMyWindowClass(HINSTANCE, LPCSTR);

void ShowPopupMenu(HWND hWnd)
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

typedef struct
{
    int     m_isInit;
    uint8_t *m_pixels;
    size_t  m_pixels_size;

    LONG    m_screenW;
    LONG    m_screenH;
    HWND    m_screenWinId;
    HDC     m_screenDC;

    HDC     m_screen_dc;
    HDC     m_screen_bitmap_dc;
    HBITMAP m_screen_bitmap;
    HGDIOBJ m_screen_null_bitmap;
} ShotData;

static void ShotData_init(ShotData *data)
{
    ZeroMemory(data, sizeof(ShotData));
    data->m_isInit = 1;
}

static void ShotData_clear(ShotData *data)
{
    if(!data->m_isInit)
        return;

    if(data->m_screen_dc)
    {
        ReleaseDC(data->m_screenWinId, data->m_screen_dc);
        data->m_screen_dc = NULL;
    }

    if(data->m_screen_null_bitmap && data->m_screen_bitmap_dc)
    {
        SelectObject(data->m_screen_bitmap_dc, data->m_screen_null_bitmap);
        data->m_screen_null_bitmap = NULL;
    }

    if(data->m_screen_bitmap_dc)
    {
        DeleteDC(data->m_screen_bitmap_dc);
        data->m_screen_bitmap_dc = NULL;
    }

    if(data->m_screen_bitmap)
    {
        DeleteObject(data->m_screen_bitmap);
        data->m_screen_bitmap = NULL;
    }

    data->m_screenWinId = NULL;

    if(data->m_screenDC)
    {
        ReleaseDC(NULL, data->m_screenDC);
        data->m_screenDC = NULL;
    }
}

static void ShotData_free(ShotData *data)
{
    if(!data->m_isInit)
        return;

    ShotData_clear(data);

    if(data->m_pixels)
    {
        free(data->m_pixels);
        data->m_pixels = NULL;
    }

    ZeroMemory(data, sizeof(ShotData));
}

static void ShotData_update(ShotData *data)
{
    RECT r;
    LONG w, h;
    size_t newSize;

    w = GetSystemMetrics(SM_CXSCREEN);
    h = GetSystemMetrics(SM_CYSCREEN);

    newSize = (w * h * 4) + w;

    if(data->m_pixels_size != newSize)
    {
        ShotData_clear(data);

        if(!data->m_pixels)
            data->m_pixels = (uint8_t*)malloc(newSize);
        else
            data->m_pixels = (uint8_t*)realloc(data->m_pixels, newSize);

        data->m_pixels_size = newSize;

        data->m_screenW = w;
        data->m_screenH = h;

        data->m_screenWinId = HWND_DESKTOP;

        data->m_screenDC = GetDC(HWND_DESKTOP);

        data->m_screen_bitmap_dc = CreateCompatibleDC(data->m_screenDC);
        data->m_screen_bitmap = CreateCompatibleBitmap(data->m_screenDC, w, h);
        data->m_screen_null_bitmap = SelectObject(data->m_screen_bitmap_dc, data->m_screen_bitmap);
        data->m_screen_dc = GetDC(data->m_screenWinId);
    }
}

typedef struct
{
    char save_path[MAX_PATH];
    uint8_t *pix_data;
    size_t pix_len;
    uint32_t w;
    uint32_t h;
    uint32_t pitch;
} SaveData;

static HANDLE s_saverThread = NULL;

DWORD WINAPI png_saver_thread(LPVOID lpParameter)
{
    SaveData *saver = (SaveData*)lpParameter;
    FILE *f;
    int raw_len;
    uint8_t *raw = stbi_write_png_to_mem(saver->pix_data, saver->pitch, saver->w, saver->h, 4, &raw_len);

    //    stbi_write_bmp("test.bmp", data->m_screenW, data->m_screenH, 4, data->m_pixels);
    if(raw)
    {
        f = fopen(saver->save_path, "wb");
        fwrite(raw, 1, raw_len, f);
        fflush(f);
        fclose(f);
    }

    free(saver->pix_data);
    free(saver);

    MessageBeep(MB_ICONASTERISK);

    return 0;
}

void closePngSaverThread()
{
    if(s_saverThread)
    {
        WaitForSingleObject(s_saverThread, INFINITE);
        CloseHandle(s_saverThread);
        s_saverThread = NULL;
    }
}

static void cmd_makeScreenshot(HWND hWnd, ShotData *data)
{
    BITMAPINFO bi;
    SaveData *saver = NULL;
    SYSTEMTIME ltime;
    uint8_t *pix8;
    uint8_t tmp;
    LONG i;

    closePngSaverThread();
    ShotData_update(data);
    BitBlt(data->m_screen_bitmap_dc, 0, 0, data->m_screenW, data->m_screenH, data->m_screen_dc, 0, 0, SRCCOPY);

    memset(&bi, 0, sizeof(BITMAPINFO));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = data->m_screenW;
    bi.bmiHeader.biHeight = -data->m_screenH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = data->m_screenW * data->m_screenH * 4;

    if(GetDIBits(data->m_screenDC, data->m_screen_bitmap, 0, data->m_screenH, data->m_pixels, &bi, DIB_RGB_COLORS) == 0)
    {
        MessageBoxA(hWnd,"Failed to take the screenshot using GetDIBits.", "Whoops", MB_OK|MB_ICONERROR);
        return;
    }

    pix8 = data->m_pixels;

    for(i = 0; i < data->m_screenW * data->m_screenH; ++i)
    {
        tmp = pix8[0];
        pix8[0] = pix8[2];
        pix8[2] = tmp;
        pix8[3] = 0xFF;
        pix8 += 4;
    }

    MessageBeep(MB_OK);

    saver = (SaveData*)malloc(sizeof(SaveData));
    if(saver)
    {
        ZeroMemory(saver, sizeof(SaveData));

        GetLocalTime(&ltime);

        saver->w = data->m_screenW;
        saver->h = data->m_screenH;
        saver->pitch = data->m_screenW * 4;
        snprintf(saver->save_path, MAX_PATH, "Scr_%04u-%02u-%02u_%02u-%02u-%02u.png",
                 ltime.wYear, ltime.wMonth, ltime.wDay,
                 ltime.wHour, ltime.wMinute, ltime.wSecond);
        saver->pix_data = malloc(data->m_pixels_size);
        saver->pix_len = data->m_pixels_size;
        memcpy(saver->pix_data,  data->m_pixels,  data->m_pixels_size);

        s_saverThread = CreateThread(NULL, 0, png_saver_thread, saver, 0, NULL);

        if(!s_saverThread)
        {
            MessageBoxA(hWnd,"Failed to make thread... running without", "Whoops", MB_OK|MB_ICONERROR);
            png_saver_thread(saver);
        }
    }

    MessageBeep(MB_ICONEXCLAMATION);
}

static ShotData s_shotData;

BOOL OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    BOOL ret = TRUE;
    // int wNotifyCode = HIWORD(wParam);   // Notification code.
    int wID         = LOWORD(wParam);   // Item, control, or accelerator identifier.
    // HWND hwndCtl    = (HWND)lParam;     // Handle of control.

    switch(wID)
    {
    case IDM_SETTINGS:
        ShowWindow(hWnd, 1);
        MessageBoxA(hWnd,"Ikon hat Kliken!", "Aktion", MB_OK|MB_ICONASTERISK);
        break;

    case IDM_SAVECLIP:
        cmd_makeScreenshot(hWnd, &s_shotData);
        break;

    case IDM_QUIT:
        if(IsWindowVisible(hWnd))
            SendMessage(hWnd, WM_DESTROY, (WPARAM)0, (LPARAM)0);
        else
            SendMessage(hWnd, WM_CLOSE, (WPARAM)0, (LPARAM)0);
        closePngSaverThread();
        break;

    case ID_CMD_MAKE_SHOT:
        cmd_makeScreenshot(hWnd, &s_shotData);
        break;

    default:
        ret = FALSE;
    }

    return ret;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_LBUTTONUP:
        MessageBoxA(hWnd,"Kliken!", "Aktion", MB_OK|MB_ICONASTERISK);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case MYWM_NOTIFYICON:
    {
        switch(lParam)
        {
        case WM_LBUTTONDBLCLK:
            ShowWindow(hWnd, 1);
            MessageBoxA(hWnd,"Ikon hat Kliken!", "Aktion", MB_OK|MB_ICONASTERISK);
            break;
        case WM_RBUTTONDOWN:
            ShowPopupMenu(hWnd);
            break;
        }

        break;
    }
    case WM_COMMAND:
        return OnCommand(hWnd, wParam, lParam);

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

ATOM RegMyWindowClass(HINSTANCE hInst, LPCSTR lpzClassName)
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

void runMsgLoop()
{
    MSG msg = {0};    //структура сообщения
    int iGetOk = 0;   //переменная состояния

    while((iGetOk = GetMessage(&msg, NULL, 0, 0 )) != 0) //цикл сообщений
    {
        if(iGetOk == -1)
            return;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static HHOOK s_msgHook = NULL;

LRESULT CALLBACK ntWindowHookLL(int code, WPARAM wParam, LPARAM lParam)
{
    if(wParam == WM_KEYUP)
    {
        KBDLLHOOKSTRUCT*s = (KBDLLHOOKSTRUCT*)lParam;
        if(s->vkCode == VK_SNAPSHOT)
            SendMessageA(s_mainWindow, WM_COMMAND, (WPARAM)ID_CMD_MAKE_SHOT, (LPARAM)0);
    }

    return CallNextHookEx(0, code, wParam, lParam);
}

static BOOL m_prScrPressed = FALSE;

void CALLBACK win9xWindowHook(HWND p1, UINT p2, UINT_PTR p3, DWORD p4)
{
    BOOL a = (GetAsyncKeyState(VK_MENU) & 0x01) == 1;
    BOOL k = (GetAsyncKeyState(VK_SNAPSHOT) & 0x01) == 1;

    (void)p1; (void)p2; (void)p3; (void)p4;

    if(a)
    {
        m_prScrPressed = FALSE;
        return;
    }

    if(!m_prScrPressed && k)
        m_prScrPressed = TRUE;
    else if(m_prScrPressed && !k)
    {
        m_prScrPressed = FALSE;
        SendMessageA(s_mainWindow, WM_COMMAND, (WPARAM)ID_CMD_MAKE_SHOT, (LPARAM)0);
    }
}

void initKeyHook(HWND hWnd, HINSTANCE hInstance)
{
    OSVERSIONINFOA osvi;
    SYSTEM_INFO sysInfo;
    int isDosBased = 0;

    ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA(&osvi);

    GetSystemInfo(&sysInfo);

    isDosBased = osvi.dwPlatformId != VER_PLATFORM_WIN32_NT;

    if(isDosBased) // Make a watch timer
    {
        SetTimer(hWnd, ID_HOOK_TIMER, 100, &win9xWindowHook);
    }
    else
    {
        s_msgHook = SetWindowsHookExA(WH_KEYBOARD_LL, ntWindowHookLL, hInstance, 0);
        if(!s_msgHook)
            MessageBoxA(NULL, "Failed to initialize the hook", "Can't init the hook", MB_OK|MB_ICONERROR);
    }
}

void closeKeyHooks(HWND hWnd)
{
    KillTimer(hWnd, ID_HOOK_TIMER);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    LPCSTR lpzClass = "My Window Class!";
    RECT screen_rect;
    struct TrayIcon icon;

    ShotData_init(&s_shotData);

    ZeroMemory(&icon, sizeof(icon));
    initLibraries();

    icon.currentShellVersion = detectShellVersion();
    icon.notifyIconSizeA = FIELD_OFFSET(NOTIFYICONDATAA, szTip[64]); // NOTIFYICONDATAA_V1_SIZE
    icon.notifyIconSizeW = FIELD_OFFSET(NOTIFYICONDATAW, szTip[64]); // NOTIFYICONDATAW_V1_SIZE;
    icon.maxTipLength = 64;

    // For restoring the tray icon after explorer crashes
    if (!MYWM_TASKBARCREATED) {
        MYWM_TASKBARCREATED = RegisterWindowMessageA("SysTrayIconClass");
    }

    if(!RegMyWindowClass(hInstance, lpzClass))
        return 1;

    GetWindowRect(GetDesktopWindow(),&screen_rect); // разрешение экрана
    int x = screen_rect.right / 2 - 150;
    int y = screen_rect.bottom / 2 - 75;

    icon.hIcon32 = (HICON)LoadImageA(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    icon.hIcon16 = (HICON)LoadImageA(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    s_mainWindow = CreateWindowA(lpzClass, "SysTrayWindow", WS_OVERLAPPEDWINDOW, x, y, 300, 150, NULL, NULL, hInstance, NULL);

    if(!s_mainWindow)
        return 2;

    SendMessage(s_mainWindow, WM_SETICON, ICON_BIG, (LPARAM)icon.hIcon32);
    SendMessage(s_mainWindow, WM_SETICON, ICON_SMALL, (LPARAM)icon.hIcon16);

    memset(&icon.tnd, 0, icon.notifyIconSizeA);
    icon.tnd.uID = 0;
    icon.tnd.cbSize = icon.notifyIconSizeA;
    icon.tnd.hWnd = s_mainWindow;
    icon.tnd.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
    icon.tnd.uCallbackMessage = MYWM_NOTIFYICON;
    icon.tnd.hIcon = icon.hIcon16;

    Shell_NotifyIconA(NIM_ADD, &icon.tnd);

    ShotData_update(&s_shotData);

    initKeyHook(s_mainWindow, hInstance);

    runMsgLoop();

    Shell_NotifyIconA(NIM_DELETE, &icon.tnd);

    ShotData_free(&s_shotData);

    return 0;
}
