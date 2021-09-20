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

#include "tiny_screenshoter.h"
#include "ui_tiny_screenshoter.h"
#include <QPixmap>
#include <QEvent>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QDateTime>
#include <QSettings>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef _WIN32
#define GLOBAL_SCREENSHOT 1000
TinyScreenshoter* TinyScreenshoter::m_this = nullptr;
HHOOK TinyScreenshoter::m_msgHook = nullptr;
#endif

void TinyScreenshoter::initHook()
{
#ifdef _WIN32
    if(QSysInfo::windowsVersion() > QSysInfo::WV_DOS_based)
    {
        HMODULE q = GetModuleHandleA(NULL);
        m_msgHook = SetWindowsHookExA(WH_KEYBOARD_LL, windowHookLL, q, 0);
    //    if(!m_msgHook)
    //        m_msgHook = SetWindowsHookExA(WH_KEYBOARD, windowHook, q, 0);
        if(!m_msgHook)
        {
            QMessageBox::warning(nullptr,
                                 "Can't init the hook",
                                 QString("Failed to initialize the hook (error %1)").arg(GetLastError()),
                                 QMessageBox::Ok);
        }
    }
#endif
}

TinyScreenshoter::TinyScreenshoter(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TinyScreenshoter)
{
    ui->setupUi(this);

#ifdef _WIN32
    m_this = this;

    // on Windows 98 run the watch timer that will watch for the PrintScreen key state actively
    if(QSysInfo::windowsVersion() < QSysInfo::WV_DOS_based)
    {
        QObject::connect(&m_watch, SIGNAL(timeout()), this, SLOT(keyWatch()));
        m_prScrPressed = false;
        m_watch.start(200);
    }
#endif

    init();
}

TinyScreenshoter::~TinyScreenshoter()
{
    delete ui;
}

void TinyScreenshoter::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void TinyScreenshoter::makeScreenshot()
{
    QPixmap okno = QPixmap::grabWindow(QApplication::desktop()->winId());
#ifdef _WIN32
    MessageBeep(MB_OK);
#endif

    QDateTime t = QDateTime::currentDateTime();
    QString saveWhere = QString("%1/Scr_%2-%3-%4-%5-%6-%7.png")
            .arg(m_savePath)
            .arg(t.date().year()).arg(t.date().month()).arg(t.date().day())
            .arg(t.time().hour()).arg(t.time().minute()).arg(t.time().second());
    okno.save(saveWhere, "PNG");
#ifdef _WIN32
    MessageBeep(MB_ICONEXCLAMATION);
#endif
}

void TinyScreenshoter::on_saveImageClipboard_clicked()
{
    if(!qApp->clipboard())
        return;

    QImage okno = qApp->clipboard()->image();

    if(okno.isNull())
    {
#ifdef _WIN32
        MessageBeep(MB_ICONERROR);
#endif
    return;
    }

#ifdef _WIN32
    MessageBeep(MB_OK);
#endif

    QDateTime t = QDateTime::currentDateTime();
    QString saveWhere = QString("%1/Scr_%2-%3-%4-%5-%6-%7.png")
            .arg(m_savePath)
            .arg(t.date().year()).arg(t.date().month()).arg(t.date().day())
            .arg(t.time().hour()).arg(t.time().minute()).arg(t.time().second());
    okno.save(saveWhere, "PNG");
#ifdef _WIN32
    MessageBeep(MB_ICONEXCLAMATION);
#endif
}


void TinyScreenshoter::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case QSystemTrayIcon::DoubleClick:
        show();
        break;
    default:
        break;
    }
}

void TinyScreenshoter::init()
{
    restoreAction = new QAction(tr("&Settings"), this);
    QObject::connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));

    saveImageClipboard = new QAction(tr("&Save image in clipboard"), this);
    QObject::connect(saveImageClipboard, SIGNAL(triggered()), this, SLOT(on_saveImageClipboard_clicked()));

    quitAction = new QAction(tr("&Quit"), this);
    QObject::connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addAction(saveImageClipboard);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(QIcon(":/ts-tray.png"));
    trayIcon->show();
    QObject::connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                     this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    QObject::connect(ui->takeScreenshot, SIGNAL(clicked()), this, SLOT(makeScreenshot()));

    loadSetup();
}

void TinyScreenshoter::loadSetup()
{
    QSettings setup(QString("%1/tinyscr.ini").arg(qApp->applicationDirPath()), QSettings::IniFormat);
    setup.beginGroup("main");
    m_savePath = setup.value("save-path", qApp->applicationDirPath()).toString();
    setup.endGroup();
}

void TinyScreenshoter::saveSetup()
{
    QSettings setup(QString("%1/tinyscr.ini").arg(qApp->applicationDirPath()), QSettings::IniFormat);
    setup.beginGroup("main");
    setup.setValue("save-path", m_savePath);
    setup.endGroup();
}

#ifdef _WIN32
LRESULT TinyScreenshoter::windowHookLL(int code, WPARAM wParam, LPARAM lParam)
{
    if(m_this != nullptr)
    {
        if(wParam == WM_KEYUP)
        {
            KBDLLHOOKSTRUCT*s = (KBDLLHOOKSTRUCT*)lParam;
            if(s->vkCode == VK_SNAPSHOT)
                QMetaObject::invokeMethod(m_this, "makeScreenshot", Qt::QueuedConnection);
        }
    }

    return CallNextHookEx(0, code, wParam, lParam);
}

LRESULT TinyScreenshoter::windowHook(int code, WPARAM wParam, LPARAM lParam)
{
    if(m_this != nullptr)
    {
        if(((lParam >> 31) & 1) == 1)
        {
            //wParam == VK_SNAPSHOT &&
//            KBDLLHOOKSTRUCT*s = (KBDLLHOOKSTRUCT*)lParam;
//            if(s->vkCode == VK_SNAPSHOT)
            QMetaObject::invokeMethod(m_this, "makeScreenshot", Qt::QueuedConnection);
        }
    }

    return CallNextHookEx(0, code, wParam, lParam);
}

void TinyScreenshoter::keyWatch()
{
    bool a = (GetAsyncKeyState(VK_MENU) & 0x01) == 1;
    bool k = (GetAsyncKeyState(VK_SNAPSHOT) & 0x01) == 1;

    if(a)
    {
        m_prScrPressed = false;
        return;
    }

    if(!m_prScrPressed && k)
    {
        m_prScrPressed = true;
    }
    else if(m_prScrPressed && !k)
    {
        m_prScrPressed = false;
        QMetaObject::invokeMethod(m_this, "makeScreenshot", Qt::QueuedConnection);
    }
}
#endif

void TinyScreenshoter::on_setSavePath_clicked()
{
    QString saveAs = QFileDialog::getExistingDirectory(NULL, "Choose the screenshot...",
                                                      m_savePath,
                                                      QFileDialog::DontUseNativeDialog|QFileDialog::ShowDirsOnly);
    if(!saveAs.isEmpty())
    {
        m_savePath = saveAs;
        saveSetup();
    }
}
