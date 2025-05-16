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

static int sendFtpCommand(char *outBuffer, size_t outBufferSize,
                          char *inBuffer, size_t inBufferSize,
                          SOCKET ftp_sock, const char *cmd, const char *data)
{
    int ret;
    outBuffer[0] = '\0';
    strncat(outBuffer, cmd, outBufferSize);
    strncat(outBuffer, " ", outBufferSize);
    strncat(outBuffer, data, outBufferSize);
    strncat(outBuffer, "\r\n", outBufferSize);
    ret = send(ftp_sock, outBuffer, strlen(outBuffer), 0);
    if(ret == SOCKET_ERROR)
        return ret;

    ZeroMemory(inBuffer, inBufferSize);
    return recv(ftp_sock, inBuffer, inBufferSize - 1, 0);
}

static int sendFtpCommandNR(char *outBuffer, size_t outBufferSize,
                            SOCKET ftp_sock, const char *cmd, const char *data)
{
    outBuffer[0] = '\0';
    strncat(outBuffer, cmd, outBufferSize);
    strncat(outBuffer, " ", outBufferSize);
    strncat(outBuffer, data, outBufferSize);
    strncat(outBuffer, "\r\n", outBufferSize);
    return send(ftp_sock, outBuffer, strlen(outBuffer), 0);
}

static int sendFtpCommandND(char *outBuffer, size_t outBufferSize,
                            char *inBuffer, size_t inBufferSize,
                            SOCKET ftp_sock, const char *cmd)
{
    int ret;
    outBuffer[0] = '\0';
    strncat(outBuffer, cmd, outBufferSize);
    strncat(outBuffer, "\r\n", outBufferSize);
    ret = send(ftp_sock, outBuffer, strlen(outBuffer), 0);
    if(ret == SOCKET_ERROR)
        return ret;

    ZeroMemory(inBuffer, inBufferSize);
    return recv(ftp_sock, inBuffer, inBufferSize, 0);
}

static uint16_t ftpParseReplyCode(const char *inBuffer, int *ok)
{
    uint16_t ret;
    const char *space;
    char out[5];

    ZeroMemory(out, 5);

    space = strchr(inBuffer, ' ');
    if(!space)
    {
        *ok = FALSE;
        return 0;
    }

    memcpy(out, inBuffer, min(space - inBuffer, 5));

    ret = (uint16_t)strtol(out, NULL, 10);

    *ok = TRUE;

    return ret;
}

static uint16_t ftpParsePassivePort(char *inBuffer, int *ok)
{
    uint16_t p_port, num_commas;
    char *str_pos = NULL, *str_tok_ptr = NULL, *str_seek;

    str_pos = strstr(inBuffer, "(");
    if(!str_pos)
    {
        *ok = FALSE;
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

    *ok = TRUE;

    return p_port;
}

static const char *ftpGetBaseName(const char* str)
{
    const char *ret = strrchr(str, '\\');

    if(!ret)
        return NULL;

    return ++ret;
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
    int try_count = 0, conn_error, res;
    uint16_t p_port = 0, reply;
    const size_t bufSizes = 1000;
    char serverMessage[1000], sendBuffer[1000];
    const char *send_file_name = NULL;
    FileSend *fileToSend = NULL;
    size_t    p_read = 0;
    FILE     *p_file = NULL;

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
    res = recv(ftp_sock, serverMessage, sizeof(serverMessage) - 1, 0);
    if(res < 0)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to receive greeting: %ld", WSAGetLastError());
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    debugLog("--FTP connected: %s\n", serverMessage);
    /* 220 (vsFTPd 3.0.5) */
    reply = ftpParseReplyCode(serverMessage, &res);
    if(reply != 220 || !res)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect FTP server", "Failed to connect to FTP server %s:%u, server reply error:\n%s",
                 g_settings.ftpHost, g_settings.ftpPort, serverMessage);
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }


    if(g_settings.ftpUser[0] != '\0')
    {
        res = sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "USER", g_settings.ftpUser);
        if(res < 0)
        {
            msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send USER command: %ld", WSAGetLastError());
            ftpCleanUp(&ftp_sock, &p_sock);
            return 0;
        }

        debugLog("--FTP Login: %s\n", serverMessage);
        /* 331 Please specify the password. */
        reply = ftpParseReplyCode(serverMessage, &res);
        if(reply != 331 || !res)
        {
            msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect FTP server", "Failed to send login %s to FTP server %s:%u, server reply error:\n%s",
                     g_settings.ftpUser, g_settings.ftpHost, g_settings.ftpPort, serverMessage);
            ftpCleanUp(&ftp_sock, &p_sock);
            return 0;
        }

        if(g_settings.ftpPassword[0] != '\0')
        {
            res = sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "PASS", g_settings.ftpPassword);
            if(res < 0)
            {
                msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send PASS command: %ld", WSAGetLastError());
                ftpCleanUp(&ftp_sock, &p_sock);
                return 0;
            }

            debugLog("--FTP Password: %s\n", serverMessage);
            /* 230 Login successful. */
            reply = ftpParseReplyCode(serverMessage, &res);
            if(reply != 230 || !res)
            {
                msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect FTP server", "Incorrect password for user %s to FTP server %s:%u, server reply error:\n%s",
                         g_settings.ftpUser, g_settings.ftpHost, g_settings.ftpPort, serverMessage);
                ftpCleanUp(&ftp_sock, &p_sock);
                return 0;
            }
        }
    }
    else
    {
        res = sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "USER", "anonymouse");
        if(res < 0)
        {
            msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send anonymous USER command: %ld", WSAGetLastError());
            ftpCleanUp(&ftp_sock, &p_sock);
            return 0;
        }

        debugLog("--FTP Anonymouse login: %s\n", serverMessage);
    }


    res = sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "CWD", g_settings.ftpSavePath);
    if(res < 0)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send CWD command: %ld", WSAGetLastError());
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    debugLog("--FTP Change dir to %s: %s\n", g_settings.ftpSavePath, serverMessage);
    /* 250 Directory successfully changed. */
    reply = ftpParseReplyCode(serverMessage, &res);
    if(reply != 250 || !res)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect FTP server", "Can't open directory %s at the FTP server %s:%u, server reply error:\n%s",
                 g_settings.ftpSavePath, g_settings.ftpHost, g_settings.ftpPort, serverMessage);
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }


    res = sendFtpCommand(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "TYPE", "I");
    if(res < 0)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send Type I command: %ld", WSAGetLastError());
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    debugLog("--FTP Type I: %s\n", serverMessage);
    /* 200 Switching to Binary mode. */
    reply = ftpParseReplyCode(serverMessage, &res);
    if(reply != 200 || !res)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect FTP server", "Can't enter binary mode at the FTP server %s:%u, server reply error:\n%s",
                 g_settings.ftpHost, g_settings.ftpPort, serverMessage);
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }


    res = sendFtpCommandND(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "PASV");
    if(res < 0)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send PASV command: %ld", WSAGetLastError());
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    debugLog("--FTP PASV: %s\n", serverMessage);
    /* 227 Entering Passive Mode (172,16,9,141,39,22). */
    reply = ftpParseReplyCode(serverMessage, &res);
    if(reply != 227 || !res)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect FTP server", "Can't enter the passive mode at the FTP server %s:%u, server reply error:\n%s",
                 g_settings.ftpHost, g_settings.ftpPort, serverMessage);
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    p_port =  ftpParsePassivePort(serverMessage, &res);
    if(!res)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONASTERISK, "Failed to send via FTP", "Failed to detect FTP mode (corrupted data received): %s", serverMessage);
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }

    debugLog("--FTP Passive Port: %u\n", p_port);


    while((fileToSend = queue_get()) != NULL)
    {
        send_file_name = ftpGetBaseName(fileToSend->filePath);
        if(!send_file_name)
        {
            errorMessageBox(NULL, "Failed to figure filename in the send file path: %s", "Can't run FTP sender");
            ftpCleanUp(&ftp_sock, &p_sock);
            free(fileToSend);
            return 0;
        }

        res = sendFtpCommandNR(sendBuffer, bufSizes, ftp_sock, "STOR", send_file_name);
        if(res < 0)
        {
            msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send STOR command: %ld", WSAGetLastError());
            ftpCleanUp(&ftp_sock, &p_sock);
            free(fileToSend);
            return 0;
        }

        debugLog("--FTP Store file: %s\n", send_file_name);

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
            {
                res = send(p_sock, sendBuffer, p_read, 0);
                if(res < 0)
                {
                    msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Failes to send data to FTP server", "Failed to send data by passive port: %ld", WSAGetLastError());
                    ftpCleanUp(&ftp_sock, &p_sock);
                    free(fileToSend);
                    return 0;
                }
            }
            fclose(p_file);
        }

        closesocket(p_sock);

        if(g_settings.ftpRemoveUploaded)
            DeleteFileA(fileToSend->filePath);

        free(fileToSend);
    }

    res = sendFtpCommandND(sendBuffer, bufSizes, serverMessage, bufSizes, ftp_sock, "QUIT");
    if(res < 0)
    {
        msgBoxPr(NULL, MB_OK|MB_ICONERROR, "Can't connect to FTP server", "Failed to send QUIT command: %ld", WSAGetLastError());
        ftpCleanUp(&ftp_sock, &p_sock);
        return 0;
    }
    debugLog("--FTP Quit: %s\n", serverMessage);
    /* 150 Ok to send data. */

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

