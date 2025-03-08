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
#include <QFtp>
#include <QtDebug>


#ifdef _WIN32
#   include <windows.h>
#   include <mmsystem.h>
#   include "spng.h"
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

    updatePixels();

    // on Windows 98 run the watch timer that will watch for the PrintScreen key state actively
    if(QSysInfo::windowsVersion() < QSysInfo::WV_DOS_based)
    {
        QObject::connect(&m_watch, SIGNAL(timeout()), this, SLOT(keyWatch()));
        m_prScrPressed = false;
        m_watch.start(200);
    }
#endif

    QObject::connect(ui->ftpHost, SIGNAL(editingFinished()),
                     this, SLOT(saveSetupSlot()));
    QObject::connect(ui->ftpPort, SIGNAL(editingFinished()),
                     this, SLOT(saveSetupSlot()));
    QObject::connect(ui->ftpUser, SIGNAL(editingFinished()),
                     this, SLOT(saveSetupSlot()));
    QObject::connect(ui->ftpPassword, SIGNAL(editingFinished()),
                     this, SLOT(saveSetupSlot()));
    QObject::connect(ui->ftpDir, SIGNAL(editingFinished()),
                     this, SLOT(saveSetupSlot()));
    QObject::connect(ui->ftpRemoveOnHost, SIGNAL(clicked()),
                     this, SLOT(saveSetupSlot()));
    QObject::connect(ui->uploadToFtp, SIGNAL(clicked()),
                     this, SLOT(saveSetupSlot()));

    m_ftpHost = new QFtp(this);
    QObject::connect(m_ftpHost, SIGNAL(commandFinished(int,bool)),
                     this, SLOT(ftpCommandFinished(int,bool)));

    init();
}

TinyScreenshoter::~TinyScreenshoter()
{
#ifdef _WIN32
    winScreenClear();
#endif
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
    setCursor(Qt::WaitCursor);

#ifdef _WIN32
    updatePixels();
    BitBlt(m_screen_bitmap_dc, 0, 0, m_screenW, m_screenH, m_screen_dc, 0, 0, SRCCOPY);

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(BITMAPINFO));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = m_screenW;
    bi.bmiHeader.biHeight = -m_screenH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = m_screenW * m_screenH * 4;

    if(GetDIBits(m_screenDC, m_screen_bitmap, 0, m_screenH, m_pixels.data(), &bi, DIB_RGB_COLORS) == 0)
    {
        setCursor(Qt::ArrowCursor);
        QMessageBox::critical(nullptr, "Whoops", "Failed to take the screenshot using GetDIBits.");
        return;
    }

    uint8_t *pix8 = m_pixels.data();
    uint8_t tmp;

    for(LONG i = 0; i < m_screenW * m_screenH; ++i)
    {
        tmp = pix8[0];
        pix8[0] = pix8[2];
        pix8[2] = tmp;
        pix8[3] = 0xFF;
        pix8 += 4;
    }

    MessageBeep(MB_OK);
#else
    QPixmap okno = QPixmap::grabWindow(QApplication::desktop()->winId());
#endif

    QDateTime t = QDateTime::currentDateTime();
    QString fName = QString("Scr_%1-%2-%3-%4-%5-%6.png")
            .arg(t.date().year(), 4, 10, QChar('0'))
            .arg(t.date().month(), 2, 10, QChar('0'))
            .arg(t.date().day(), 2, 10, QChar('0'))
            .arg(t.time().hour(), 2, 10, QChar('0'))
            .arg(t.time().minute(), 2, 10, QChar('0'))
            .arg(t.time().second(), 2, 10, QChar('0'));
    QString saveWhere = QString("%1/%2")
            .arg(m_savePath)
            .arg(fName);

#ifdef _WIN32
    struct spng_ihdr ihdr;
    spng_ctx *ctx;
    int ret;

    memset(&ihdr, 0, sizeof(ihdr));
    std::wstring upath = fName.toStdWString();

    FILE *f = _wfopen(upath.c_str(), L"wb");
    if(f)
    {
        ctx = spng_ctx_new(SPNG_CTX_ENCODER);
        if(ctx)
        {
            ihdr.width = m_screenW;
            ihdr.height = m_screenH;
            ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
            ihdr.bit_depth = 8;

            spng_set_ihdr(ctx, &ihdr);
            spng_set_png_file(ctx, f);

            ret = spng_encode_image(ctx, m_pixels.data(), m_pixels.size(), SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE);

            if(ret)
                QMessageBox::critical(nullptr, "PNG Encode error", spng_strerror(ret));

            spng_ctx_free(ctx);
        }

        fclose(f);
    }

    MessageBeep(MB_ICONEXCLAMATION);
#else
    okno.save(saveWhere, "PNG");
#endif

    if(ui->uploadToFtp->isChecked())
        ftpUpload(saveWhere, fName);

    setCursor(Qt::ArrowCursor);
}

void TinyScreenshoter::on_saveImageClipboard_clicked()
{
    if(!qApp->clipboard())
        return;

    setCursor(Qt::WaitCursor);

    QImage okno = qApp->clipboard()->image();

    if(okno.isNull())
    {
        setCursor(Qt::ArrowCursor);
#ifdef _WIN32
        MessageBeep(MB_ICONERROR);
#endif
        return;
    }

#ifdef _WIN32
    MessageBeep(MB_OK);
#endif

    QDateTime t = QDateTime::currentDateTime();
    QString fName = QString("Scr_%1-%2-%3-%4-%5-%6.png")
            .arg(t.date().year(), 4, 10, QChar('0'))
            .arg(t.date().month(), 2, 10, QChar('0'))
            .arg(t.date().day(), 2, 10, QChar('0'))
            .arg(t.time().hour(), 2, 10, QChar('0'))
            .arg(t.time().minute(), 2, 10, QChar('0'))
            .arg(t.time().second(), 2, 10, QChar('0'));
    QString saveWhere = QString("%1/%2")
            .arg(m_savePath)
            .arg(fName);

    okno.save(saveWhere, "PNG");

#ifdef _WIN32
    MessageBeep(MB_ICONEXCLAMATION);
#endif

    if(ui->uploadToFtp->isChecked())
        ftpUpload(saveWhere, fName);

    setCursor(Qt::ArrowCursor);
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

    setup.beginGroup("ftp");
    ui->uploadToFtp->setChecked(setup.value("enable", false).toBool());
    ui->ftpRemoveOnHost->setChecked(setup.value("remove-files", false).toBool());
    ui->ftpHost->setText(setup.value("host", QString()).toString());
    ui->ftpUser->setText(setup.value("user", QString()).toString());
    ui->ftpPassword->setText(setup.value("password", QString()).toString());
    ui->ftpPort->setValue(setup.value("port", 21).toInt());
    ui->ftpDir->setText(setup.value("dir", QString()).toString());
    setup.endGroup();
}

void TinyScreenshoter::saveSetup()
{
    QSettings setup(QString("%1/tinyscr.ini").arg(qApp->applicationDirPath()), QSettings::IniFormat);
    setup.beginGroup("main");
    setup.setValue("save-path", m_savePath);
    setup.endGroup();

    setup.beginGroup("ftp");
    setup.setValue("enable", ui->uploadToFtp->isChecked());
    setup.setValue("remove-files", ui->ftpRemoveOnHost->isChecked());
    setup.setValue("host", ui->ftpHost->text());
    setup.setValue("port", ui->ftpPort->value());
    setup.setValue("user", ui->ftpUser->text());
    setup.setValue("password", ui->ftpPassword->text());
    setup.setValue("dir", ui->ftpDir->text());
    setup.endGroup();
}

void TinyScreenshoter::ftpUpload(QString path, QString fName)
{
    ui->ftpLog->clear();
    m_ftpHost->connectToHost(ui->ftpHost->text(), (quint16)ui->ftpPort->value());
    m_ftpHost->login(ui->ftpUser->text(), ui->ftpPassword->text());

    if(!ui->ftpDir->text().isEmpty())
        m_ftpHost->cd(ui->ftpDir->text());
    else
        m_ftpHost->cd(".");

    m_uploadingFile.setFileName(path);
    m_uploadingFile.open(QIODevice::ReadOnly);
    m_ftpHost->put(&m_uploadingFile, fName);
    m_ftpHost->close();
}

void TinyScreenshoter::ftpCommandFinished(int id, bool error)
{
    switch(m_ftpHost->currentCommand())
    {
    case QFtp::ConnectToHost:// connect
        if(error)
        {
            ui->ftpLog->append(QString("Can't connect FTP: %1\n").arg(m_ftpHost->errorString()));
            m_ftpHost->close();
            break;
        }
        ui->ftpLog->append("-- Connected");
        break;

    case QFtp::Login:// login
        if(error)
        {
            ui->ftpLog->append(QString("FTP login failed: %1").arg(m_ftpHost->errorString()));
            m_ftpHost->close();
            break;
        }
        ui->ftpLog->append("-- Logged in");
        break;

    case QFtp::Cd:// cd
        if(error)
        {
            ui->ftpLog->append(QString("FTP cd failed: %1").arg(m_ftpHost->errorString()));
            m_ftpHost->close();
            break;
        }
        ui->ftpLog->append("-- cd done");
        break;

    case QFtp::Put://put
        if(error)
        {
            ui->ftpLog->append(QString("FTP put failed: %1").arg(m_ftpHost->errorString()));
            m_ftpHost->close();
            break;
        }
        ui->ftpLog->append("-- Put completed");
        break;

    case QFtp::Close://close
        m_uploadingFile.close();
        if(ui->ftpRemoveOnHost->isChecked())
            m_uploadingFile.remove();
        m_uploadingFile.setFileName(QString());
        ui->ftpLog->append("-- Closed");
        break;

    default:
        break;
    }
}

void TinyScreenshoter::saveSetupSlot()
{
    saveSetup();
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

#ifdef _WIN32
void TinyScreenshoter::winScreenClear()
{
    if(m_screen_dc)
    {
        ReleaseDC(m_screenWinId, m_screen_dc);
        m_screen_dc = nullptr;
    }

    if(m_screen_null_bitmap && m_screen_bitmap_dc)
    {
        SelectObject(m_screen_bitmap_dc, m_screen_null_bitmap);
        m_screen_null_bitmap = nullptr;
    }

    if(m_screen_bitmap_dc)
    {
        DeleteDC(m_screen_bitmap_dc);
        m_screen_bitmap_dc = nullptr;
    }

    if(m_screen_bitmap)
    {
        DeleteObject(m_screen_bitmap);
        m_screen_bitmap = nullptr;
    }

    m_screenWinId = nullptr;

    if(m_screenDC)
    {
        ReleaseDC(nullptr, m_screenDC);
        m_screenDC = nullptr;
    }
}

void TinyScreenshoter::updatePixels()
{
    RECT r;
    WId winId = QApplication::desktop()->winId();
    GetClientRect(winId, &r);

    LONG w = r.right - r.left;
    LONG h = r.bottom - r.top;

    int newSize = (w * h * 4);

    if(m_pixels.size() != newSize)
    {
        winScreenClear();

        m_pixels.resize(newSize);
        m_screenW = w;
        m_screenH = h;

        m_screenWinId = winId;

        m_screenDC = GetDC(0);

        m_screen_bitmap_dc = CreateCompatibleDC(m_screenDC);
        m_screen_bitmap = CreateCompatibleBitmap(m_screenDC, w, h);
        m_screen_null_bitmap = SelectObject(m_screen_bitmap_dc, m_screen_bitmap);
        m_screen_dc = GetDC(m_screenWinId);
    }
}
#endif

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
