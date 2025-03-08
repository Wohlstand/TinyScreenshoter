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

#include <stdlib.h>
#include <windows.h>

#include "shot_data.h"

ShotData g_shotData;


void ShotData_init(ShotData *data)
{
    ZeroMemory(data, sizeof(ShotData));
    data->m_isInit = 1;
}

void ShotData_free(ShotData *data)
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


void ShotData_clear(ShotData *data)
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

void ShotData_update(ShotData *data)
{
    LONG w, h;
    size_t newSize;

    w = GetSystemMetrics(SM_CXSCREEN);
    h = GetSystemMetrics(SM_CYSCREEN);

    newSize = (w * h * 4);

    if(data->m_pixels_size != newSize)
    {
        ShotData_clear(data);

        if(!data->m_pixels)
            data->m_pixels = (uint8_t*)malloc(newSize + w);
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
