#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLabel>
#include <QImageReader>
#include <QBuffer>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QPainter>
#include <QDebug>

#include <memory>

#include "videodecoder.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_jpegMgr(nullptr)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
    if (m_jpegMgr)
    {
        delete m_jpegMgr;
    }
}

void MainWindow::SetImage(const QString& imgPath)
{
    QImageReader imgReader( imgPath );
    imgReader.setAutoTransform( true );
    QImage qimg = imgReader.read();

    ui->photoVidget->SetPixmap(QPixmap::fromImage(qimg));
    if (imgPath.endsWith("jpeg") || imgPath.endsWith("jpg"))
    {
        m_jpegMgr = new JpegManager(imgPath);
        if (m_jpegMgr->HasMicrovideo())
        {
            std::shared_ptr<VideoDecoder> vv (new VideoDecoder (m_jpegMgr->getVideoMemory()));
            if (vv->isVideoInited())
            {
                ui->photoVidget->SetPlayButtonVisible(true);
                ui->photoVidget->SetVideoManager(vv);
            }
        }
    }
}
