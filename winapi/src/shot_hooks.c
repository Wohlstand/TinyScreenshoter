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

#include <windows.h>

#include "shot_hooks.h"
#include "tray_icon.h"
#include "resource.h"
#include "resource_ex.h"


static HHOOK    s_msgHook = NULL;
static BOOL     s_prScrPressed = FALSE;


LRESULT CALLBACK ntWindowHookLL(int code, WPARAM wParam, LPARAM lParam)
{
    if(wParam == WM_KEYUP)
    {
        KBDLLHOOKSTRUCT*s = (KBDLLHOOKSTRUCT*)lParam;
        if(s->vkCode == VK_SNAPSHOT)
            SendMessageA(g_trayIconHWnd, WM_COMMAND, (WPARAM)ID_CMD_MAKE_SHOT, (LPARAM)0);
    }

    return CallNextHookEx(0, code, wParam, lParam);
}

void CALLBACK win9xWindowHook(HWND p1, UINT p2, UINT_PTR p3, DWORD p4)
{
    BOOL a = (GetAsyncKeyState(VK_MENU) & 0x01) == 1;
    BOOL k = (GetAsyncKeyState(VK_SNAPSHOT) & 0x01) == 1;

    (void)p1; (void)p2; (void)p3; (void)p4;

    if(a)
    {
        s_prScrPressed = FALSE;
        return;
    }

    if(!s_prScrPressed && k)
        s_prScrPressed = TRUE;
    else if(s_prScrPressed && !k)
    {
        s_prScrPressed = FALSE;
        SendMessageA(g_trayIconHWnd, WM_COMMAND, (WPARAM)ID_CMD_MAKE_SHOT, (LPARAM)0);
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

    if(isDosBased) /* Make a watch timer */
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
