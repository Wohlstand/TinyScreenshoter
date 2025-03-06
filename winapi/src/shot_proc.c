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
#include <stdint.h>
#include <windows.h>

#include "shot_proc.h"
#include "shot_data.h"
#include "settings.h"
#include "misc.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


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
    /* stbi_write_bmp("test.bmp", data->m_screenW, data->m_screenH, 4, data->m_pixels); */

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

void cmd_makeScreenshot(HWND hWnd, ShotData *data)
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
        errorMessageBox(hWnd, "Failed to take the screenshot using GetDIBits: %s", "Whoops");
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
        snprintf(saver->save_path, MAX_PATH, "%s\\Scr_%04u-%02u-%02u_%02u-%02u-%02u.png",
                 g_settings.savePath,
                 ltime.wYear, ltime.wMonth, ltime.wDay,
                 ltime.wHour, ltime.wMinute, ltime.wSecond);
        saver->pix_data = malloc(data->m_pixels_size);
        saver->pix_len = data->m_pixels_size;
        memcpy(saver->pix_data,  data->m_pixels,  data->m_pixels_size);

        s_saverThread = CreateThread(NULL, 0, png_saver_thread, saver, 0, NULL);

        if(!s_saverThread)
        {
            errorMessageBox(hWnd, "Failed to make thread: %s.\n\nTrying without.", "Whoops");
            png_saver_thread(saver);
        }
    }

    MessageBeep(MB_ICONEXCLAMATION);
}
