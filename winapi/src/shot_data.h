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

#ifndef SHOT_DATA_H
#define SHOT_DATA_H

#include <stddef.h>
#include <stdint.h>
#include <windef.h>

struct ShotData
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
};

typedef struct ShotData ShotData;

extern ShotData g_shotData;


void ShotData_init(ShotData *data);
void ShotData_free(ShotData *data);

void ShotData_clear(ShotData *data);
void ShotData_update(ShotData *data);

#endif /* SHOT_DATA_H */
