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

static DWORD WINAPI ftp_sender_thread(LPVOID lpParameter)
{
    WSADATA w_data;
    SOCKET ftp_sock, p_sock;
    SOCKADDR_IN server, p_server;
    int try_count = 0, conn_error, num_commas;
    uint16_t p_port = 0;
    char serverMessage[1000], sendBuffer[1000];
    char *str_pos = NULL, *str_tok_ptr = NULL, *str_seek;
    FileSend *fileToSend = NULL;
    size_t      p_read = 0;
    FILE       *p_file = NULL;

    (void)lpParameter;

    WSAStartup(MAKEWORD(2, 2), &w_data);

    ftp_sock = socket(2, SOCK_STREAM, IPPROTO_TCP);
    if(ftp_sock == INVALID_SOCKET)
    {
        errorMessageBox(NULL, "Failed to initialize WinSock: %s", "Can't run FTP sender");
        WSACleanup();
        queue_clear();
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
            errorMessageBox(NULL, "Failed to connect the FTP host (10 attempts failed): %s", "Can't run FTP sender");
            closesocket(ftp_sock);
            WSACleanup();
            queue_clear();
            return 0;
        }
    }

    ZeroMemory(serverMessage, sizeof(serverMessage));
    recv(ftp_sock, serverMessage, sizeof(serverMessage), 0);

    printf("--FTP connected: %s\n", serverMessage);
    fflush(stdout);

    if(g_settings.ftpUser[0] != '\0')
    {
        sendBuffer[0] = '\0';
        strncat(sendBuffer, "USER ", 1000);
        strncat(sendBuffer, g_settings.ftpUser, 1000);
        strncat(sendBuffer, "\r\n", 1000);
        send(ftp_sock, sendBuffer, strlen(sendBuffer), 0);
        ZeroMemory(serverMessage, sizeof(serverMessage));
        recv(ftp_sock, serverMessage, 1000, 0);
        printf("--FTP Login: %s\n", serverMessage);
        fflush(stdout);

        sendBuffer[0] = '\0';
        strncat(sendBuffer, "PASS ", 1000);
        strncat(sendBuffer, g_settings.ftpPassword, 1000);
        strncat(sendBuffer, "\r\n", 1000);
        send(ftp_sock, sendBuffer, strlen(sendBuffer), 0);
        ZeroMemory(serverMessage, sizeof(serverMessage));
        recv(ftp_sock, serverMessage, 1000, 0);
        printf("--FTP Password: %s\n", serverMessage);
        fflush(stdout);
    }

    sendBuffer[0] = '\0';
    strncat(sendBuffer, "CWD ", 1000);
    strncat(sendBuffer, g_settings.ftpSavePath, 1000);
    strncat(sendBuffer, "\r\n", 1000);
    send(ftp_sock, sendBuffer, strlen(sendBuffer), 0);
    ZeroMemory(serverMessage, sizeof(serverMessage));
    recv(ftp_sock, serverMessage, 1000, 0);
    printf("--FTP Change dir to %s: %s\n", g_settings.ftpSavePath, serverMessage);
    fflush(stdout);

    send(ftp_sock, "TYPE I\r\n", 8, 0);
    ZeroMemory(serverMessage, sizeof(serverMessage));
    recv(ftp_sock, serverMessage, 1000, 0);
    printf("--FTP Type I: %s\n", serverMessage);
    fflush(stdout);

    send(ftp_sock, "PASV\r\n", 6, 0);
    ZeroMemory(serverMessage, sizeof(serverMessage));
    recv(ftp_sock, serverMessage, 1000, 0);
    printf("--FTP PASV: %s\n", serverMessage);
    fflush(stdout);

    str_pos = strstr(serverMessage, "(");
    if(!str_pos)
    {
        errorMessageBox(NULL, "Failed to detect FTP mode (corrupted data received): %s", "Can't run FTP sender");
        closesocket(ftp_sock);
        WSACleanup();
        queue_clear();
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

    printf("--FTP Passive Port: %u\n", p_port);
    fflush(stdout);


    while((fileToSend = queue_get()) != NULL)
    {
        sendBuffer[0] = '\0';
        strncat(sendBuffer, "STOR ", 1000);
        str_pos = strrchr(fileToSend->filePath, '\\');

        if(!str_pos)
        {
            errorMessageBox(NULL, "Failed to figure filename in the send file path: %s", "Can't run FTP sender");
            closesocket(ftp_sock);
            WSACleanup();
            free(fileToSend);
            queue_clear();
            return 0;
        }

        strncat(sendBuffer, str_pos + 1, 1000);
        strncat(sendBuffer, "\r\n", 1000);
        send(ftp_sock, sendBuffer, strlen(sendBuffer), 0);

        p_sock = socket(2, SOCK_STREAM, IPPROTO_TCP);
        if(p_sock == INVALID_SOCKET)
        {
            errorMessageBox(NULL, "Failed to connect passive port: %s", "Can't run FTP sender");
            closesocket(ftp_sock);
            WSACleanup();
            free(fileToSend);
            queue_clear();
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
                closesocket(p_sock);
                closesocket(ftp_sock);
                WSACleanup();
                free(fileToSend);
                queue_clear();
                return 0;
            }
        }

        p_file = fopen(fileToSend->filePath, "rb");
        if(p_file)
        {
            while((p_read = fread(sendBuffer, 1, 1000, p_file)) > 0)
                send(p_sock, sendBuffer, p_read, 0);
            fclose(p_file);
        }

        closesocket(p_sock);

        if(g_settings.ftpRemoveUploaded)
            DeleteFileA(fileToSend->filePath);

        free(fileToSend);
    }

    sendBuffer[0] = '\0';
    strncat(sendBuffer, "QUIT\r\n", 1000);
    send(ftp_sock, sendBuffer, strlen(sendBuffer), 0);
    ZeroMemory(serverMessage, sizeof(serverMessage));
    recv(ftp_sock, serverMessage, 1000, 0);
    printf("--FTP Quit: %s\n", serverMessage);
    fflush(stdout);

    closesocket(ftp_sock);
    WSACleanup();

    return 0;
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
        ftp_sender_thread(NULL);
}
