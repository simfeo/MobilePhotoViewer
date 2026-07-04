#include "imagerenderwidget.h"
#include "ui_imagerenderwidget.h"

#include <QPainter>

#include <QAudioFormat>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QAudioSink>
#include <QMediaDevices>
#include <QBuffer>

#include <QDebug>

ImageRenderWidget::ImageRenderWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ImageRenderWidget)
    , m_stillImage(nullptr)
    , m_nextImage(nullptr)
{
    m_ui->setupUi(this);
    m_ui->playButton->hide();


    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    m_videoWidget = new VideoWidget(this);
    m_player->setVideoOutput(m_videoWidget);
    m_videoWidget->hide();
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &ImageRenderWidget::statusChanged);
}

ImageRenderWidget::~ImageRenderWidget()
{
    m_player->stop();
    delete m_ui;
}

void ImageRenderWidget::SetPixmap(const QPixmap& image)
{
    if (image.isNull())
    {
        qDebug() << "invalid image on input";
    }
    m_stillImage.reset(new QPixmap(image));
    m_player->stop();
    m_videoWidget->hide();
    m_ui->playButton->hide();
}

void ImageRenderWidget::SetPlayButtonVisible(bool state)
{
    m_ui->playButton->setVisible(state);
}

void ImageRenderWidget::SetVideoManager(std::shared_ptr<QBuffer> videoBuff)
{
    QBuffer* btArray = new QBuffer(this);
    btArray->setData(videoBuff->data());
    m_player->setSourceDevice(btArray);
}

void ImageRenderWidget::paintEvent(QPaintEvent* event)
{
    (void)event;
    if (m_stillImage && !m_nextImage)
    {
        QPainter painter(this);
        auto m = m_stillImage->scaled(width(), height(), Qt::KeepAspectRatio);
        int x_offset = (width() - m.width()) / 2;
        int y_offset = (height() - m.height()) / 2;
        painter.drawPixmap(x_offset, y_offset, m);
        m_videoWidget->setGeometry(x_offset, y_offset, m.width(), m.height());
    }
    else if (m_nextImage)
    {
        QPainter painter(this);
        QPixmap dstPix = QPixmap::fromImage(*m_nextImage.get());
        auto m = dstPix.scaled(width(), height(), Qt::KeepAspectRatio);
        int x_offset = (width() - m.width()) / 2;
        int y_offset = (height() - m.height()) / 2;
        painter.drawPixmap(x_offset, y_offset, m);
        m_videoWidget->setGeometry(x_offset, y_offset, m.width(), m.height());
    }
}

void ImageRenderWidget::statusChanged(QMediaPlayer::MediaStatus status)
{
    // handle status message
    switch (status) {
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::LoadedMedia:
        // setStatusInfo(QString());
        m_videoWidget->hide();
        m_ui->playButton->show();
        break;
    case QMediaPlayer::LoadingMedia:
        // setStatusInfo(tr("Loading..."));
        break;
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::BufferedMedia:
        // setStatusInfo(tr("Buffering %1%").arg(qRound(m_player->bufferProgress() * 100.)));
        break;
    case QMediaPlayer::StalledMedia:
        // setStatusInfo(tr("Stalled %1%").arg(qRound(m_player->bufferProgress() * 100.)));
        break;
    case QMediaPlayer::EndOfMedia:
        // QApplication::alert(this);
        // m_playlist->next();
        m_ui->playButton->show();
        m_videoWidget->hide();
        break;
    case QMediaPlayer::InvalidMedia:
        // displayErrorMessage();
        break;
    }
}

void ImageRenderWidget::on_playButton_clicked()
{
    m_ui->playButton->hide();
    m_videoWidget->show();
    m_player->play();
}
