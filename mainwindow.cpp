#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLabel>
#include <QImageReader>
#include <QBuffer>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QPainter>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QWheelEvent>

#include <QFile>
#include <QFileInfo>
#include <QDir>

#include <QDebug>

#include <memory>
#include <math.h>

#include "videodecoder.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_jpegMgr(nullptr)
    , m_heifMgr(nullptr)
    , m_currentPath()
{
    ui->setupUi(this);
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    delete ui;
    if (m_jpegMgr)
    {
        delete m_jpegMgr;
    }

    if (m_heifMgr)
    {
        delete m_heifMgr;
    }
}

void MainWindow::SetImage(const QString& imgPath)
{
    ui->photoVidget->SetPlayButtonVisible(false);
    QImageReader imgReader(imgPath);
    imgReader.setAutoTransform(true);
    QImage qimg = imgReader.read();
    m_currentPath = imgPath;

    ui->photoVidget->SetPixmap(QPixmap::fromImage(qimg));
    if (imgPath.endsWith("jpeg") || imgPath.endsWith("jpg"))
    {
        m_jpegMgr = new JpegManager(imgPath);
        if (m_jpegMgr->HasMicrovideo())
        {
            std::shared_ptr<VideoDecoder> vv(new VideoDecoder(m_jpegMgr->getVideoMemory()));
            if (vv->isVideoInited())
            {
                ui->photoVidget->SetPlayButtonVisible(true);
                ui->photoVidget->SetVideoManager(vv);
            }
        }
    }
    else if (imgPath.endsWith("heic") || imgPath.endsWith("heif"))
    {
        m_heifMgr = new HeifManager(imgPath);

        if (m_heifMgr->getWidth()  && m_heifMgr->getHeight())
        {
            QImage qimg = QImage(m_heifMgr->getDecodedImage().get(), static_cast<int>(m_heifMgr->getWidth()), static_cast<int>(m_heifMgr->getHeight()), QImage::Format_RGBA8888);
            ui->photoVidget->SetPixmap(QPixmap::fromImage(qimg));
        }
        if (m_heifMgr->HasMicrovideo())
        {
            std::shared_ptr<VideoDecoder> vv(new VideoDecoder(m_heifMgr->getVideoMemory()));
            if (vv->isVideoInited())
            {
                ui->photoVidget->SetPlayButtonVisible(true);
                ui->photoVidget->SetVideoManager(vv);
            }
        }
    }
    repaint();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();

       // check for our needed mime type, here a file or a list of files
       if (mimeData->hasUrls())
       {
         QStringList pathList;
         QList<QUrl> urlList = mimeData->urls();

         QString path = urlList.front().toLocalFile();
         SetImage(path);
       }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    QPoint numDegrees = event->angleDelta() / 8;
    QPoint numSteps = numDegrees / 15;
    if (numSteps.y() != 0 && m_currentPath.size() != 0)
    {
        QDir workingDir = QFileInfo( m_currentPath ).absoluteDir();
        QStringList images = workingDir.entryList(QStringList() << "*.jpg" << "*.JPG",QDir::Files);
        if (images.empty())
        {
            return;
        }
        images.sort();
        int in = images.indexOf(QFileInfo( m_currentPath ).fileName());
        const int current_in = in;
        in = in - numSteps.y();
        if (in < 0)
        {
            in = 0;
        }
        else if (in >= images.size())
        {
            in = images.size()-1;
        }

        if (current_in != in)
        {
            SetImage(workingDir.absoluteFilePath(images[in]));
        }
    }
}
