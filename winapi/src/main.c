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

#include "shot_proc.h"
#include "shot_data.h"
#include "shot_hooks.h"
#include "ftp_sender.h"
#include "tray_icon.h"
#include "settings.h"


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


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = 0;

    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    InitCommonControls();

    shotProc_init();
    ftpSender_init();
    settingsInit(hInstance);
    ShotData_init(&g_shotData);

    ret = initSysTrayIcon(hInstance);
    if(ret != 0)
        return ret;

    ShotData_update(&g_shotData);

    initKeyHook(g_trayIconHWnd, hInstance);

    runMsgLoop();

    settingsDestroy();
    closeSysTrayIcon();
    shotProc_quit();
    ftpSender_quit();

    ShotData_free(&g_shotData);

    return 0;
}
