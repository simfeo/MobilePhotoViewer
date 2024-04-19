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

#include <jpegmanager.h>
#include <heifmanager.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentPath()
    , m_imageManager(nullptr)
{
    ui->setupUi(this);
    setAcceptDrops(true);
    for (auto& i : QImageReader::supportedImageFormats())
    {
        m_supportedFormats.append("*."+i);
    }
    m_supportedFormats.append("*.heic");
    m_supportedFormats.append("*.heif");
}

MainWindow::~MainWindow()
{
    delete ui;
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
        m_imageManager.reset(new JpegManager(imgPath));
    }
    else if (imgPath.endsWith("heic") || imgPath.endsWith("heif"))
    {
        m_imageManager.reset(new HeifManager(imgPath));

        if (m_imageManager->getWidth()  && m_imageManager->getHeight())
        {
            QImage qimg = QImage(m_imageManager->getDecodedImage().get(), static_cast<int>(m_imageManager->getWidth()), static_cast<int>(m_imageManager->getHeight()), QImage::Format_RGBA8888);
            ui->photoVidget->SetPixmap(QPixmap::fromImage(qimg));
        }
    }
    else
    {
        m_imageManager.reset(new ImageWithVideoBase());
    }
    if (m_imageManager->HasMicrovideo())
    {
        ui->photoVidget->SetVideoManager(m_imageManager->getVideoMemory());
    }
    setWindowTitle(imgPath);
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
        QStringList images = workingDir.entryList(m_supportedFormats,QDir::Files);
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
