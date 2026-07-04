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
    // Fill the still-photo rectangle exactly. A motion-photo video shares the
    // still's framing, so ignoring the (sub-percent) aspect difference matches
    // the photo without a visible letterbox.
    m_videoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
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

// Rectangle the current image is drawn into: scaled to fit while keeping the
// aspect ratio, then centred. The video overlay is sized to this same rect.
QRect ImageRenderWidget::currentImageRect() const
{
    QSize src;
    if (m_nextImage)
        src = m_nextImage->size();
    else if (m_stillImage)
        src = m_stillImage->size();

    if (src.isEmpty())
        return QRect();

    QSize scaled = src.scaled(size(), Qt::KeepAspectRatio);
    int x_offset = (width() - scaled.width()) / 2;
    int y_offset = (height() - scaled.height()) / 2;
    return QRect(x_offset, y_offset, scaled.width(), scaled.height());
}

void ImageRenderWidget::updateVideoGeometry()
{
    const QRect r = currentImageRect();
    if (r.isValid())
        m_videoWidget->setGeometry(r);
}

void ImageRenderWidget::paintEvent(QPaintEvent* event)
{
    (void)event;
    const QRect r = currentImageRect();
    if (!r.isValid())
        return;

    QPainter painter(this);
    if (m_nextImage)
        painter.drawPixmap(r, QPixmap::fromImage(*m_nextImage.get()));
    else
        painter.drawPixmap(r, *m_stillImage);
}

void ImageRenderWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Keep the video overlay aligned with the photo on every size change.
    // Doing this here (rather than in paintEvent) keeps the native video
    // surface in sync even while a video is playing and covering the widget.
    updateVideoGeometry();
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
    updateVideoGeometry(); // ensure the overlay matches the photo before showing
    m_videoWidget->show();
    m_player->play();
}
