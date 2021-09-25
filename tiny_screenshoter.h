/*
 * MIT License
 *
 * Copyright (c) 2018 Vitaly Novichkov
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

#ifndef TINY_SCREENSHOTER_H
#define TINY_SCREENSHOTER_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QFile>

class QFtp;

namespace Ui {
class TinyScreenshoter;
}

class TinyScreenshoter : public QMainWindow
{
    Q_OBJECT

public:
    static void initHook();
    explicit TinyScreenshoter(QWidget *parent = 0);
    ~TinyScreenshoter();

protected:
    void changeEvent(QEvent *e);

private slots:
    void makeScreenshot();
    void on_saveImageClipboard_clicked();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void on_setSavePath_clicked();
#ifdef Q_OS_WIN
    void keyWatch();
#endif
    void ftpCommandFinished(int id, bool error);
    void saveSetupSlot();

private:
    void init();

    void loadSetup();
    void saveSetup();

    void ftpUpload(QString path, QString fName);

#ifdef _WIN32
    static LRESULT CALLBACK windowHookLL(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK windowHook(int code, WPARAM wParam, LPARAM lParam);
    static TinyScreenshoter* m_this;
    static HHOOK m_msgHook;
    QTimer m_watch;
    bool m_prScrPressed = false;
#endif

    QString m_savePath;
    QFile m_uploadingFile;

    Ui::TinyScreenshoter *ui;

    QAction *restoreAction;
    QAction *quitAction;
    QAction *saveImageClipboard;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QFtp *m_ftpHost = nullptr;
};

#endif // TINY_SCREENSHOTER_H
