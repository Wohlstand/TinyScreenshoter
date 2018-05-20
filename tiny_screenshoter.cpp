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
#include <QDesktopWidget>
#include <QFileDialog>
#include <QDateTime>
#include <QThread>

TinyScreenshoter::TinyScreenshoter(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TinyScreenshoter)
{
    ui->setupUi(this);
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

/*
 WORKAROUND to escape "protected" limitation and use that freaking static function!
 */
class Kek : public QThread
{
public:
    static void wait(unsigned long)
    {
        QThread::msleep(200);
    }
};

void TinyScreenshoter::on_makeShot_clicked()
{
    this->hide();
    qApp->processEvents();
#ifdef _WIN32
    Sleep(3000);
#else
    Kek::wait(200);
#endif

    QPixmap okno = QPixmap::grabWindow(QApplication::desktop()->winId());
    QDateTime t = QDateTime::currentDateTime();
    QString saveWhere = qApp->applicationDirPath() + QString("/Scr_%1-%2-%3-%4-%5-%6.png")
            .arg(t.date().year()).arg(t.date().month()).arg(t.date().day())
            .arg(t.time().hour()).arg(t.time().minute()).arg(t.time().second());
    QString saveAs = QFileDialog::getSaveFileName(NULL, "Save screenshot as...",
                                                  saveWhere,
                                                  "PNG Picture (*.png)",
                                                  nullptr,
                                                  QFileDialog::DontUseNativeDialog);
    if(!saveAs.isEmpty())
        okno.save(saveAs, "PNG");

    this->show();
}
