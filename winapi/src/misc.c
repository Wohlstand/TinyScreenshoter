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


void errorMessageBox(HWND hWnd, const char *errorFormat, const char *msgBoxTitle)
{
    char outBuffer[1024];
    char errBuffer[1024];
    DWORD errId = GetLastError();
    LPSTR messageBuffer = NULL;
    size_t msgSize;

    ZeroMemory(outBuffer, sizeof(outBuffer));
    ZeroMemory(errBuffer, sizeof(errBuffer));

    if(errId > 0)
    {
        msgSize = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, errId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
        memcpy(errBuffer, messageBuffer, min(msgSize, 1023));
        LocalFree(messageBuffer);
    }
    else
        snprintf(errBuffer, 1024, "<Unknown error>");

    snprintf(outBuffer, 1024, errorFormat, errBuffer);

    MessageBoxA(hWnd, outBuffer, msgBoxTitle, MB_OK|MB_ICONERROR);
}

char *shot_strtokr(char *s1, const char *s2, char **ptr)
{
    const char *p = s2;

    if(!s2 || !ptr || (!s1 && !*ptr))
        return NULL;

    if(s1 != NULL)  /* new string */
        *ptr = s1;
    else   /* old string continued */
    {
        if(*ptr == NULL)
        {
        /* No old string, no new string, nothing to do */
            return NULL;
        }
        s1 = *ptr;
    }

    /* skip leading s2 characters */
    while(*p && *s1)
    {
        if(*s1 == *p)
        {
        /* found separator; skip and start over */
            ++s1;
            p = s2;
            continue;
        }
        ++p;
    }

    if(! *s1) /* no more to parse */
    {
        *ptr = s1;
        return NULL;
    }

    /* skipping non-s2 characters */
    *ptr = s1;

    while(**ptr)
    {
        p = s2;

        while (*p)
        {
            if(**ptr == *p++)
            {
            /* found separator; overwrite with '\0', position *ptr, return */
                *((*ptr)++) = '\0';
                return s1;
            }
        }

        ++(*ptr);
    }

    /* parsed to end of string */
    return s1;
}
