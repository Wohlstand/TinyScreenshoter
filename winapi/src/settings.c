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
#include <shlobj.h>
#include "shot_data.h"
#include "shot_proc.h"
#include "tray_icon.h"
#include "misc.h"
#include "resource.h"
#include "settings.h"


static char s_configFilePath[MAX_PATH];
static char s_configDir[MAX_PATH];
TinyShotSettings g_settings;

static HWND s_settingsDialogue = NULL;
static HINSTANCE s_instance = NULL;


void settingsInit(HINSTANCE inst)
{
    char exePath[MAX_PATH];
    size_t i, len;

    s_instance = inst;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    ZeroMemory(&g_settings, sizeof(TinyShotSettings));
    ZeroMemory(s_configFilePath, MAX_PATH);
    ZeroMemory(exePath, MAX_PATH);

    GetModuleFileNameA(inst, exePath, MAX_PATH - 1);

    len = strlen(exePath);

    for(i = len - 1; i >= 0; --i)
    {
        if(exePath[i] == '\\' || exePath[i] == '/')
        {
            exePath[i] = '\0';
            break;
        }
    }

    strncpy(s_configDir, exePath, MAX_PATH);
    snprintf(s_configFilePath, MAX_PATH, "%s\\tinyscr_w.ini", exePath);

    settingsLoad();
}

static void touchConfigFile()
{
    FILE *f = fopen(s_configFilePath, "r");
    if(!f)
    {
        f = fopen(s_configFilePath, "w");
        if(!f)
        {
            MessageBoxA(NULL, "Can't create the tinyscr_w.ini config file!", "Error", MB_OK|MB_ICONERROR);
            return;
        }
        fclose(f);
    }
    else
        fclose(f);
}

static BOOL writeIniInt(const char *section, const char *key, int value, const char *file)
{
    char out[25];
    snprintf(out, 25, "%d", value);
    return WritePrivateProfileStringA(section, key, out, file);
}

void settingsLoad()
{
    touchConfigFile();

    GetPrivateProfileStringA("main", "save-path", s_configDir, g_settings.savePath, MAX_PATH, s_configFilePath);

    g_settings.ftpEnable = GetPrivateProfileIntA("ftp", "enable", FALSE, s_configFilePath);
    g_settings.ftpRemoveUploaded = GetPrivateProfileIntA("ftp", "remove-files", FALSE, s_configFilePath);
    GetPrivateProfileStringA("ftp", "host", "", g_settings.ftpHost, 120, s_configFilePath);
    g_settings.ftpPort = GetPrivateProfileIntA("ftp", "port", 21, s_configFilePath);

    GetPrivateProfileStringA("ftp", "user", "", g_settings.ftpUser, 120, s_configFilePath);
    GetPrivateProfileStringA("ftp", "password", "", g_settings.ftpPassword, 120, s_configFilePath);
    GetPrivateProfileStringA("ftp", "dir", "", g_settings.ftpSavePath, 120, s_configFilePath);
}

void settingsSave()
{
    touchConfigFile();

    WritePrivateProfileStringA("main", "save-path", g_settings.savePath, s_configFilePath);

    writeIniInt("ftp", "enable", g_settings.ftpEnable, s_configFilePath);
    writeIniInt("ftp", "remove-files", g_settings.ftpRemoveUploaded, s_configFilePath);
    WritePrivateProfileStringA("ftp", "host", g_settings.ftpHost, s_configFilePath);
    writeIniInt("ftp", "port", g_settings.ftpPort, s_configFilePath);

    WritePrivateProfileStringA("ftp", "user", g_settings.ftpUser, s_configFilePath);
    WritePrivateProfileStringA("ftp", "password", g_settings.ftpPassword, s_configFilePath);
    WritePrivateProfileStringA("ftp", "dir", g_settings.ftpSavePath, s_configFilePath);
}

static int CALLBACK SeelctDirCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    (void)lParam;

    if(uMsg == BFFM_INITIALIZED)
        SendMessageA(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)(LPCTSTR)(lpData));

    return 0;
}

static void syncFtpEnable(HWND hwnd)
{
    EnableWindow(GetDlgItem(hwnd, ID_FTP_HOST), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, ID_FTP_PORT), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, ID_FTP_USER), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, ID_FTP_PASSWORD), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, ID_FTP_DIRECTORY), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, ID_FTP_REMOVE_POST), g_settings.ftpEnable);

    EnableWindow(GetDlgItem(hwnd, FTP_LABEL_HOST), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, FTP_LABEL_PORT), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, FTP_LABEL_USER), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, FTP_LABEL_PASSWORD), g_settings.ftpEnable);
    EnableWindow(GetDlgItem(hwnd, FTP_LABEL_DIR), g_settings.ftpEnable);
}

static void syncWidget(HWND hwnd)
{
    char num[6];
    SendDlgItemMessageA(hwnd, ID_FTP_ENABLE, BM_SETCHECK, g_settings.ftpEnable, 0);
    syncFtpEnable(hwnd);
    SendDlgItemMessageA(hwnd, ID_FTP_HOST, WM_SETTEXT, (WPARAM)NULL, (LPARAM)g_settings.ftpHost);
    snprintf(num, 6, "%u", g_settings.ftpPort);
    SendDlgItemMessageA(hwnd, ID_FTP_PORT, WM_SETTEXT, (WPARAM)NULL, (LPARAM)num);
    SendDlgItemMessageA(hwnd, ID_FTP_USER, WM_SETTEXT, (WPARAM)NULL, (LPARAM)g_settings.ftpUser);
    SendDlgItemMessageA(hwnd, ID_FTP_PASSWORD, WM_SETTEXT, (WPARAM)NULL, (LPARAM)g_settings.ftpPassword);
    SendDlgItemMessageA(hwnd, ID_FTP_DIRECTORY, WM_SETTEXT, (WPARAM)NULL, (LPARAM)g_settings.ftpSavePath);
    SendDlgItemMessageA(hwnd, ID_FTP_REMOVE_POST, BM_SETCHECK, g_settings.ftpRemoveUploaded, 0);
}

static BOOL CALLBACK SettingsDialogueProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    BROWSEINFO ofn;
    LPITEMIDLIST pidl;
    char buff_s[MAX_PATH];
    uint16_t buff_u = 0;

    switch(iMsg)
    {
    case WM_INITDIALOG:
        SendMessageA(hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_trayIcon.hIcon32);
        SendMessageA(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_trayIcon.hIcon16);
        SetWindowPos(hDlg, HWND_TOP, 100, 100, 0, 0, SWP_NOSIZE);
        syncWidget(hDlg);
        break;

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        settingsDestroy();
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_BUTTON_TAKE_SHOT:
            cmd_makeScreenshot(hDlg, &g_shotData);
            break;

        case ID_BUTTON_SAVE_CLIP:
            cmd_dumpClipboard(hDlg, &g_shotData);
            break;

        case ID_BUTTON_SET_PATH:
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.hwndOwner = hDlg;
            ofn.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            ofn.lParam = (LPARAM)g_settings.savePath;
            ofn.lpfn = SeelctDirCallback;

            pidl = SHBrowseForFolderA(&ofn);

            if(pidl != 0)
            {
                /* get the name of the folder and put it in path */
                SHGetPathFromIDListA(pidl, g_settings.savePath);
                CoTaskMemFree(pidl);
                settingsSave();
            }

            break;

        case ID_FTP_ENABLE:
            if(HIWORD(wParam) == BN_CLICKED)
            {
                g_settings.ftpEnable = SendDlgItemMessageA(hDlg, ID_FTP_ENABLE, (UINT)BM_GETCHECK, 0, 0);
                syncFtpEnable(hDlg);
                settingsSave();
            }
            break;

        case ID_FTP_REMOVE_POST:
            if(HIWORD(wParam) == BN_CLICKED)
            {
                g_settings.ftpRemoveUploaded = SendDlgItemMessageA(hDlg, ID_FTP_REMOVE_POST, (UINT)BM_GETCHECK, 0, 0);
                settingsSave();
            }
            break;

        case ID_FTP_HOST:
            if(HIWORD(wParam) == EN_KILLFOCUS)
            {
                GetDlgItemTextA(hDlg, ID_FTP_HOST, buff_s, 120);

                if(strncmp(g_settings.ftpHost, buff_s, 120) != 0)
                {
                    strncpy(g_settings.ftpHost, buff_s, 120);
                    printf("FTP host field changed: %s\n", g_settings.ftpHost);
                    fflush(stdout);
                    settingsSave();
                }
            }
            break;

        case ID_FTP_PORT:
            if(HIWORD(wParam) == EN_KILLFOCUS)
            {
                GetDlgItemTextA(hDlg, ID_FTP_PORT, buff_s, 6);
                buff_u = (uint16_t)strtoul(buff_s, NULL, 10);

                if(g_settings.ftpPort != buff_u)
                {
                    g_settings.ftpPort = buff_u;
                    printf("FTP port field changed: %u\n", g_settings.ftpPort);
                    fflush(stdout);
                    settingsSave();
                }
            }
            break;

        case ID_FTP_USER:
            if(HIWORD(wParam) == EN_KILLFOCUS)
            {
                GetDlgItemTextA(hDlg, ID_FTP_USER, buff_s, 120);

                if(strncmp(g_settings.ftpUser, buff_s, 120) != 0)
                {
                    strncpy(g_settings.ftpUser, buff_s, 120);
                    printf("FTP user field changed: %s\n", g_settings.ftpUser);
                    fflush(stdout);
                    settingsSave();
                }
            }
            break;

        case ID_FTP_PASSWORD:
            if(HIWORD(wParam) == EN_KILLFOCUS)
            {
                GetDlgItemTextA(hDlg, ID_FTP_PASSWORD, buff_s, 120);

                if(strncmp(g_settings.ftpPassword, buff_s, 120) != 0)
                {
                    strncpy(g_settings.ftpPassword, buff_s, 120);
                    printf("FTP password field changed: %s\n", g_settings.ftpPassword);
                    fflush(stdout);
                    settingsSave();
                }
            }
            break;

        case ID_FTP_DIRECTORY:
            if(HIWORD(wParam) == EN_KILLFOCUS)
            {
                GetDlgItemTextA(hDlg, ID_FTP_DIRECTORY, buff_s, MAX_PATH);

                if(strncmp(g_settings.ftpSavePath, buff_s, MAX_PATH) != 0)
                {
                    strncpy(g_settings.ftpSavePath, buff_s, MAX_PATH);
                    printf("FTP password field changed: %s\n", g_settings.ftpPassword);
                    fflush(stdout);
                    settingsSave();
                }
            }
            break;
        }
        break;

    default:
        break;
    }

    return FALSE;
}

void settingsOpen()
{
    if(IsWindow(s_settingsDialogue))
    {
        ShowWindow(s_settingsDialogue, SW_SHOW);
        return;
    }

    s_settingsDialogue = CreateDialogA(s_instance, MAKEINTRESOURCEA(IDD_SETTINGS_DIALOGUE), NULL, (DLGPROC)SettingsDialogueProc);
    if(!s_settingsDialogue)
    {
        errorMessageBox(NULL, "Failed to create settings dialogue: %s", "Error");
        return;
    }

    ShowWindow(s_settingsDialogue, SW_SHOW);
}

void settingsDestroy()
{
    if(IsWindow(s_settingsDialogue))
    {
        DestroyWindow(s_settingsDialogue);
        s_settingsDialogue = NULL;
    }
}
