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
#include <windef.h>
#include <windows.h>
#include <winsock.h>
#include "misc.h"
#include "tray_icon.h"
#include "shot_hooks.h"
#include "settings.h"
#include "ftp_sender.h"


typedef struct tagFileSend
{
    char filePath[MAX_PATH];
    struct tagFileSend *b_next;
    struct tagFileSend *b_prev;
} FileSend;

static FileSend* s_queue_begin = NULL;
static FileSend* s_queue_end = NULL;
static HANDLE s_queue_mutex = 0;

static void queue_insert(FileSend *item)
{
    if(s_queue_mutex)
        WaitForSingleObject(s_queue_mutex, INFINITE);

    if(!s_queue_begin) /* First item */
    {
        s_queue_begin = item;
        s_queue_end = item;
    }
    else
    {
        s_queue_end->b_next = item;
        item->b_prev = s_queue_end;
        s_queue_end = item;
    }

    if(s_queue_mutex)
        ReleaseMutex(s_queue_mutex);
}

static FileSend *queue_get()
{
    FileSend *ret = NULL;

    if(s_queue_mutex)
        WaitForSingleObject(s_queue_mutex, INFINITE);

    if(s_queue_begin)
    {
        ret = s_queue_begin;
        s_queue_begin = s_queue_begin->b_next;

        if(!s_queue_begin) /* Reached end of queue */
            s_queue_end = NULL;
        else
            s_queue_begin->b_prev = NULL;
    }

    if(s_queue_mutex)
        ReleaseMutex(s_queue_mutex);

    return ret;
}

static void queue_clear()
{
    FileSend *fileToSend = NULL;

    while((fileToSend = queue_get()) != NULL)
    {
        free(fileToSend);
    }
}


static HANDLE s_senderThread = NULL;
static DWORD s_senderThreadId = 0;

static void sendFtpCommand(char *outBuffer, size_t outBufferSize,
                           char *inBuffer, size_t inBufferSize,
                           SOCKET ftp_sock, const char *cmd, const char *data)
{
    outBuffer[0] = '\0';
    strncat(outBuffer, cmd, outBufferSize);
    strncat(outBuffer, " ", outBufferSize);
    strncat(outBuffer, data, outBufferSize);
    strncat(outBuffer, "\r\n", outBufferSize);
    send(ftp_sock, outBuffer, strlen(outBuffer), 0);

    ZeroMemory(inBuffer, inBufferSize);
    recv(ftp_sock, inBuffer, inBufferSize, 0);
}

static void sendFtpCommandNR(char *outBuffer, size_t outBufferSize,
                             SOCKET ftp_sock, const char *cmd, const char *data)
{
    outBuffer[0] = '\0';
    strncat(outBuffer, cmd, outBufferSize);
    strncat(outBuffer, " ", outBufferSize);
    strncat(outBuffer, data, outBufferSize);
    strncat(outBuffer, "\r\n", outBufferSize);
    send(ftp_sock, outBuffer, strlen(outBuffer), 0);
}

static void sendFtpCommandND(char *outBuffer, size_t outBufferSize,
                             char *inBuffer, size_t inBufferSize,
                             SOCKET ftp_sock, const char *cmd)
{
    outBuffer[0] = '\0';
    strncat(outBuffer, cmd, outBufferSize);
    strncat(outBuffer, "\r\n", outBufferSize);
    send(ftp_sock, outBuffer, strlen(outBuffer), 0);

    ZeroMemory(inBuffer, inBufferSize);
    recv(ftp_sock, inBuffer, inBufferSize, 0);
}

static void ftpCleanUp(SOCKET *ftp_sock, SOCKET *p_sock)
{
    if(*p_sock)
    {
        closesocket(*p_sock);
        *p_sock = 0;
    }

    if(*ftp_sock)
    {
        closesocket(*ftp_sock);
        *ftp_sock = 0;
    }

    WSACleanup();
    queue_clear();
}

static DWORD WINAPI ftp_sender_thread(LPVOID lpParameter)
{
    WSADATA w_data;
    SOCKET ftp_sock = 0, p_sock = 0;
    SOCKADDR_IN server, p_server;
    int try_count = 0, conn_error, num_commas, res;
    uint16_t p_port = 0;
    const size_t bufSizes = 1000;
    char serverMessage[bufSizes], sendBuffer[bufSizes];
    char *str_pos = NULL, *str_tok_ptr = NULL, *str_seek;
    FileSend *fileToSend = NULL;
    size_t      p_read = 0;
    FILE       *p_file = NULL;

    (void)lpParameter;

    res = WSAStartup(MAKEWORD(2, 2), &w_data);
    if(res != NO_ERROR)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't initialize WinSock for FTP sender", "Failed to initialize WinSock: Error %d", res);
        WSACleanup();
        queue_clear();
        return 0;
    }

    ftp_sock = socket(2, SOCK_STREAM, IPPROTO_TCP);
    if(ftp_sock == INVALID_SOCKET)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't create socket for FTP sender", "Failed to create TCP socket: %ld", WSAGetLastError());
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    server.sin_family = 2;
    server.sin_port = htons(g_settings.ftpPort);
    server.sin_addr.s_addr = inet_addr(g_settings.ftpHost);

    conn_error = connect(ftp_sock, (LPSOCKADDR)&server, sizeof(struct sockaddr));
    while(conn_error == SOCKET_ERROR)
    {
        conn_error = connect(ftp_sock, (LPSOCKADDR)&server, sizeof(struct sockaddr));
        try_count++;
        if(try_count >= 10)
        {
            msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect FTP server", "Failed to connect to FTP server %s:%u, with error %ld",
                     g_settings.ftpHost, g_settings.ftpPort, WSAGetLastError());
            ftpCleanUp(&ftp_sock, &p_sock);
            return 0;
        }
    }

    ZeroMemory(serverMessage, sizeof(serverMessage));
    recv(ftp_sock, serverMessage, sizeof(serverMessage), 0);

    debugLog("--FTP connected: %s\n", serverMessage);

    if(g_settings.ftpUser[0] != '\0')
    {
        sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "USER", g_settings.ftpUser);
        debugLog("--FTP Login: %s\n", serverMessage);
        // 331 Please specify the password.

        sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "PASS", g_settings.ftpPassword);
        debugLog("--FTP Password: %s\n", serverMessage);
        // 230 Login successful.
    }
    else
    {
        sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "USER", "anonymouse");
        debugLog("--FTP Anonymouse login: %s\n", serverMessage);
    }

    sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "CWD", g_settings.ftpSavePath);
    debugLog("--FTP Change dir to %s: %s\n", g_settings.ftpSavePath, serverMessage);

    sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "TYPE", "I");
    debugLog("--FTP Type I: %s\n", serverMessage);
    // 200 Switching to Binary mode.

    sendFtpCommandND(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "PASV");
    debugLog("--FTP PASV: %s\n", serverMessage);
    // 227 Entering Passive Mode (172,16,9,141,39,22).

    str_pos = strstr(serverMessage, "(");
    if(!str_pos)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONASTERISK, "Failed to send via FTP", "Failed to detect FTP mode (corrupted data received): %s", serverMessage);
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    str_pos++;
    num_commas = 0;
    p_port = 0;

    str_seek = shot_strtokr(str_pos, ",", &str_tok_ptr);
    while(str_seek)
    {
        if(num_commas == 4)
            p_port = (uint16_t)strtoul(str_seek, NULL, 10) * 256;
        else if(num_commas == 5)
            p_port += (uint16_t)strtoul(str_seek, NULL, 10);
        str_seek = shot_strtokr(NULL, ",", &str_tok_ptr);
        num_commas++;
    }

    debugLog("--FTP Passive Port: %u\n", p_port);


    while((fileToSend = queue_get()) != NULL)
    {
        str_pos = strrchr(fileToSend->filePath, '\\');
        if(!str_pos)
        {
            errorMessageBox(NULL, "Failed to figure filename in the send file path: %s", "Can't run FTP sender");
            ftpCleanUp(&ftp_sock, &p_sock);
            free(fileToSend);
            return 0;
        }

        str_pos++;

        sendFtpCommandNR(sendBuffer, bufSizes, ftp_sock, "STOR", str_pos);
        debugLog("--FTP Store file: %s\n", str_pos);

        p_sock = socket(2, SOCK_STREAM, IPPROTO_TCP);
        if(p_sock == INVALID_SOCKET)
        {
            errorMessageBox(NULL, "Failed to connect passive port: %s", "Can't run FTP sender");
            ftpCleanUp(&ftp_sock, &p_sock);
            free(fileToSend);
            return 0;
        }

        p_server.sin_family = 2;
        p_server.sin_port = htons(p_port);
        p_server.sin_addr.s_addr = inet_addr(g_settings.ftpHost);

        try_count = 0;
        conn_error = connect(p_sock, (LPSOCKADDR)&p_server, sizeof(struct sockaddr));
        while(conn_error == SOCKET_ERROR)
        {
            conn_error = connect(p_sock, (LPSOCKADDR)&p_server, sizeof(struct sockaddr));
            try_count++;

            if(try_count >= 10)
            {
                errorMessageBox(NULL, "Failed to connect passive port: %s", "Can't run FTP sender");
                ftpCleanUp(&ftp_sock, &p_sock);
                free(fileToSend);
                return 0;
            }
        }

        p_file = fopen(fileToSend->filePath, "rb");
        if(p_file)
        {
            while((p_read = fread(sendBuffer, 1, bufSizes, p_file)) > 0)
                send(p_sock, sendBuffer, p_read, 0);
            fclose(p_file);
        }

        closesocket(p_sock);

        if(g_settings.ftpRemoveUploaded)
            DeleteFileA(fileToSend->filePath);

        free(fileToSend);
    }

    sendFtpCommandND(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "QUIT");
    debugLog("--FTP Quit: %s\n", serverMessage);
    // 150 Ok to send data.

    ftpCleanUp(&ftp_sock, &p_sock);

    return 0;
}

BOOL ftpSender_isBusy()
{
    DWORD res = 0;

    if(s_senderThread)
        res = WaitForSingleObject(s_senderThread, 0);
    else
        res = WAIT_OBJECT_0;

    return res != WAIT_OBJECT_0;
}

void ftpSender_init()
{
    if(!s_queue_mutex)
        s_queue_mutex = CreateMutexA(NULL, FALSE, NULL);
}

void ftpSender_quit()
{
    if(s_senderThread)
    {
        WaitForSingleObject(s_senderThread, INFINITE);
        CloseHandle(s_senderThread);
        s_senderThread = NULL;
    }

    if(s_queue_mutex)
    {
        CloseHandle(s_queue_mutex);
        s_queue_mutex = 0;
    }
}

static BOOL tryRunFtpThread(HWND hWnd)
{
    DWORD res = 0;

    if(s_senderThread)
        res = WaitForSingleObject(s_senderThread, 0);
    else
        res = WAIT_OBJECT_0;

    if(res == WAIT_OBJECT_0)
    {
        s_senderThread = CreateThread(NULL, 0, &ftp_sender_thread, NULL, 0, &s_senderThreadId);
        if(!s_senderThread)
        {
            errorMessageBox(hWnd, "Failed to make FTP sender thread: %s\n\nTrying without.", "Whoops");
            return FALSE;
        }
    }

    return TRUE;
}

void ftpSender_queueFile(HWND hWnd, const char *filePath)
{
    FileSend *fileToSend = (FileSend *)malloc(sizeof(FileSend));
    ZeroMemory(fileToSend, sizeof(FileSend));
    strncpy(fileToSend->filePath, filePath, MAX_PATH);
    queue_insert(fileToSend);

    if(!tryRunFtpThread(hWnd))
    {
        sysTraySetIcon(SET_ICON_UPLOAD);
        ftp_sender_thread(NULL);
        sysTraySetIcon(SET_ICON_NORMAL);
    }
    else
        initIconBlinker(hWnd);
}

