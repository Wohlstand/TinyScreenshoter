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

#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <stdint.h>
#include <windef.h>
#include <shlwapi.h>

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

typedef struct TrayIcon TrayIcon;


#define MYWM_NOTIFYICON (WM_APP + 101)


extern HWND g_trayIconHWnd;
extern TrayIcon g_trayIcon;


void initLibraries();
int detectShellVersion();

int initSysTrayIcon(HINSTANCE hInstance);
void closeSysTrayIcon();

#endif /* TRAY_ICON_H */
