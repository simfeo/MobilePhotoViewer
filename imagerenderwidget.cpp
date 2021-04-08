#include "imagerenderwidget.h"
#include "ui_imagerenderwidget.h"
#include "videodecoder.h"

#include <QPainter>
#include <QDebug>
#include <thread>
#include <mutex>
#include <chrono>

namespace  {
bool thread_alive = true;
std::mutex m;
}

ImageRenderWidget::ImageRenderWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::ImageRenderWidget)
    , m_stillImage(nullptr)
    , m_videoDecoder(nullptr)
    , m_nextImage(nullptr)
    , m_videoUpdater(nullptr)
{
    m_ui->setupUi(this);
    m_ui->playButton->hide();
}

ImageRenderWidget::~ImageRenderWidget()
{
    delete m_ui;
    if (m_videoUpdater)
    {
        thread_alive = false;
        m_videoUpdater.reset();
    }
}

void ImageRenderWidget::SetPixmap(const QPixmap& image)
{

    m_stillImage.reset(new QPixmap(image));
}

void ImageRenderWidget::SetPlayButtonVisible(bool state)
{
    m_ui->playButton->setVisible(state);
}

void ImageRenderWidget::SetVideoManager(std::shared_ptr<VideoDecoder> videoDecoder)
{
    m_videoDecoder = videoDecoder;
}

void ImageRenderWidget::paintEvent(QPaintEvent *event)
{

    std::lock_guard<std::mutex> lock (m);
    (void)event;
    if (m_stillImage && !m_nextImage)
    {
        QPainter painter(this);
        painter.drawPixmap(0,0, m_stillImage->scaled(width(),height(), Qt::KeepAspectRatio));
        //        painter.setPen(Qt::blue);
        //        painter.setFont(QFont("Arial", 30));
        //        painter.drawText(rect(), Qt::AlignCenter, "Qt");
    }
    else if (m_nextImage)
    {
        QPainter painter(this);
        QPixmap dstPix = QPixmap::fromImage(*m_nextImage.get());
        painter.drawPixmap(0,0, dstPix.scaled(width(),height(), Qt::KeepAspectRatio));
    }
}

void ImageRenderWidget::on_playButton_clicked()
{
    m_ui->playButton->hide();
    m_videoUpdater.reset(new std::thread([this]()
    {
        size_t videoWidth = m_videoDecoder->getMetadataInfo().width;
        size_t videoHeight = m_videoDecoder->getMetadataInfo().height;
        size_t threadSleepTime = m_videoDecoder->getMetadataInfo().frameDuration * 1000000;

        m_videoDecoder->startAudioAndVideoDecoding();
        size_t frameShowed = 0;
        std::shared_ptr<uint8_t> imgBuf;
        while (thread_alive)
        {
            auto imgBufTmp = m_videoDecoder->popVideoFrame();
            if (imgBufTmp)
            {
                ++frameShowed;
                {
                    std::lock_guard<std::mutex> lock (m);
                    imgBuf = imgBufTmp;
                    m_nextImage.reset(new QImage(imgBuf.get(), static_cast<int>(videoWidth), static_cast<int>(videoHeight), QImage::Format_RGBA8888));

                    QPoint center = m_nextImage->rect().center();
                    QTransform matrix;
                    matrix.translate(center.x(), center.y());
                    matrix.rotate(90);
                    m_nextImage.reset( new QImage(m_nextImage->transformed(matrix)) );
                }

                QMetaObject::invokeMethod(this, "repaint", Qt::QueuedConnection);
            }
            else if (frameShowed)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(threadSleepTime));
        }
        if (thread_alive)
        {
            std::lock_guard<std::mutex> lock (m);
            m_nextImage.reset();
        }
        if (thread_alive)
        {
            //            m_ui->playButton->show();
            QMetaObject::invokeMethod(m_ui->playButton, "show", Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, "repaint", Qt::QueuedConnection);
        }
    }
    )
                         , [](std::thread* p){p->join(); delete p;}
    );

    //    emit playVideoButtonClickedSignal();
}
