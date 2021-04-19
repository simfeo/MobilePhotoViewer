#include "imagerenderwidget.h"
#include "ui_imagerenderwidget.h"
#include "videodecoder.h"

#include <QPainter>
#include <QAudioOutput>
#include <QDebug>
#include <thread>
#include <mutex>
#include <chrono>

namespace {
    bool thread_alive = true;
    std::mutex m;
}

ImageRenderWidget::ImageRenderWidget(QWidget* parent)
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
    thread_alive = false;

    if (m_videoUpdater)
    {
        m_videoUpdater.reset();
    }
    if (m_audioUpdater)
    {
        m_audioUpdater.reset();
    }
}

void ImageRenderWidget::SetPixmap(const QPixmap& image)
{
    if (image.isNull())
    {
        qDebug() << "invalid image on input";
    }
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

void ImageRenderWidget::startVideoThread()
{
    m_videoUpdater.reset(new std::thread([this]()
        {
            size_t videoWidth = m_videoDecoder->getMetadataInfo().width;
            size_t videoHeight = m_videoDecoder->getMetadataInfo().height;
            size_t threadSleepTime = m_videoDecoder->getMetadataInfo().frameDuration * 1000000;
            auto chrono_sleep_time = std::chrono::microseconds(threadSleepTime);

            m_videoDecoder->startAudioAndVideoDecoding();
            size_t frameShowed = 0;
            std::shared_ptr<uint8_t> imgBuf;
            if (m_videoDecoder->hasAudioSource())
            {
                std::thread st_audio([this]() {startAudioThread(); });
                st_audio.detach();
            }
            //            auto video_playback_start_time = std::chrono::steady_clock::now();
            auto frame_decoding_start_time = std::chrono::steady_clock::now();
            auto frame_decoding_end_time = std::chrono::steady_clock::now();
            auto frame_decoding_duration = frame_decoding_start_time - frame_decoding_start_time;

            while (thread_alive)
            {
                frame_decoding_start_time = std::chrono::steady_clock::now();
                bool is_thread_alive = true;
                auto imgBufTmp = m_videoDecoder->popVideoFrame(is_thread_alive);
                if (imgBufTmp)
                {
                    ++frameShowed;
                    {
                        std::lock_guard<std::mutex> lock(m);
                        imgBuf = imgBufTmp;
                        m_nextImage.reset(new QImage(imgBuf.get(), static_cast<int>(videoWidth), static_cast<int>(videoHeight), QImage::Format_RGBA8888));

                        if (m_videoDecoder->getMetadataInfo().rotation)
                        {
                            QPoint center = m_nextImage->rect().center();
                            QTransform matrix;
                            matrix.translate(center.x(), center.y());
                            matrix.rotate(m_videoDecoder->getMetadataInfo().rotation);
                            m_nextImage.reset(new QImage(m_nextImage->transformed(matrix)));
                        }
                    }

                    QMetaObject::invokeMethod(this, "repaint", Qt::QueuedConnection);
                }
                else if (is_thread_alive)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                else if (frameShowed)
                {
                    break;
                }
                frame_decoding_end_time = std::chrono::steady_clock::now();
                frame_decoding_duration = frame_decoding_duration + std::chrono::duration_cast<std::chrono::milliseconds>(frame_decoding_end_time - frame_decoding_start_time);
                //                qDebug()<<"rescaling time " << frame_decoding_duration.count()<< (chrono_sleep_time - frame_decoding_duration).count();

                std::this_thread::sleep_for(frame_decoding_duration > chrono_sleep_time ? std::chrono::milliseconds(1) : chrono_sleep_time - frame_decoding_duration);

                frame_decoding_duration = std::chrono::steady_clock::now() - frame_decoding_end_time - chrono_sleep_time + frame_decoding_duration;
            }
            if (thread_alive)
            {
                std::lock_guard<std::mutex> lock(m);
                m_nextImage.reset();
            }
            //            auto video_playback_end_time = std::chrono::steady_clock::now();
            //            auto total_video_playback_time = std::chrono::duration_cast<std::chrono::milliseconds>(video_playback_end_time - video_playback_start_time);
            //            qDebug()<<"video playback time " << total_video_playback_time.count();
            if (thread_alive)
            {
                //            m_ui->playButton->show();
                QMetaObject::invokeMethod(m_ui->playButton, "show", Qt::QueuedConnection);
                QMetaObject::invokeMethod(this, "repaint", Qt::QueuedConnection);
            }
        }
    )
        , [](std::thread* p) {p->join(); delete p; }
        );

}

void ImageRenderWidget::startAudioThread()
{
    std::shared_ptr<std::thread> m_audioUpdater(new std::thread([this]()
        {
            QAudioFormat fmt;
            fmt.setSampleRate(48000);
            fmt.setSampleSize(16);
            fmt.setChannelCount(2);
            fmt.setCodec("audio/pcm");
            fmt.setByteOrder(QAudioFormat::LittleEndian);
            fmt.setSampleType(QAudioFormat::UnSignedInt);
            std::shared_ptr<QAudioOutput>output(new QAudioOutput(fmt));
            std::shared_ptr<QIODevice> audio_io(output->start());

            auto pcm_frames = m_videoDecoder->getPcmFrames();
            size_t index = 0;
            size_t max_size = pcm_frames.size();
            auto start = std::chrono::steady_clock::now();
            while (thread_alive)
            {
                audio_io->write(reinterpret_cast<char*>(&pcm_frames[index].buffer[0]), pcm_frames[index].buffer.size());
                ++index;
                if (index == max_size)
                {
                    break;
                }
                while (thread_alive && static_cast<size_t>(output->bytesFree()) < pcm_frames[index].buffer.size())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
            if (thread_alive)
            {
                auto end = std::chrono::steady_clock::now();
                auto t_delta = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                int should_wait = pcm_frames.back().duration + pcm_frames.back().start_time - t_delta.count();
                if (should_wait > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(should_wait));
                }
            }
            audio_io.reset();
            output.reset();
        })
        , [](std::thread* p) {p->join(); delete p; }
        );
}

void ImageRenderWidget::paintEvent(QPaintEvent* event)
{

    std::lock_guard<std::mutex> lock(m);
    (void)event;
    if (m_stillImage && !m_nextImage)
    {
        QPainter painter(this);
        auto m = m_stillImage->scaled(width(), height(), Qt::KeepAspectRatio);
        int x_offset = (width() - m.width()) / 2;
        int y_offset = (height() - m.height()) / 2;
        painter.drawPixmap(x_offset, y_offset, m);
        //        painter.setPen(Qt::blue);
        //        painter.setFont(QFont("Arial", 30));
        //        painter.drawText(rect(), Qt::AlignCenter, "Qt");
    }
    else if (m_nextImage)
    {
        QPainter painter(this);
        QPixmap dstPix = QPixmap::fromImage(*m_nextImage.get());
        auto m = dstPix.scaled(width(), height(), Qt::KeepAspectRatio);
        int x_offset = (width() - m.width()) / 2;
        int y_offset = (height() - m.height()) / 2;
        painter.drawPixmap(x_offset, y_offset, m);
    }
}

void ImageRenderWidget::on_playButton_clicked()
{
    m_ui->playButton->hide();
    std::thread st_video([this]() {startVideoThread(); });
    st_video.detach();

    //    emit playVideoButtonClickedSignal();
}
