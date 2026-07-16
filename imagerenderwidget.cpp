#include "imagerenderwidget.h"
#include "ui_imagerenderwidget.h"

#include <QPainter>

#include <QAudioFormat>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QAudioSink>
#include <QMediaDevices>
#include <QBuffer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>

#include <QDebug>

ImageRenderWidget::ImageRenderWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ImageRenderWidget)
    , m_stillImage(nullptr)
    , m_nextImage(nullptr)
    , m_zoom(1.0)
    , m_imgTopLeft(0, 0)
    , m_dragging(false)
    , m_hasVideo(false)
{
    m_ui->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);

    // Motion-photo controls: a semi-transparent pill in the photo's top-left
    // (positioned in positionPlayButton()) holding play/pause and mute, rather
    // than a single round button over the photo.
    static const char* kPillButtonQss =
        "QPushButton{background:transparent;color:white;border:none;"
        "border-radius:11px;font-size:14px;min-width:22px;min-height:22px;}"
        "QPushButton:hover{background:rgba(255,255,255,45);}";
    static const char* kPillLabelQss = "color:white;font-size:12px;font-weight:bold;";

    m_muteButton = new QPushButton(this);
    m_muteButton->setText(QString::fromUtf8("\xF0\x9F\x94\x8A"));   // 🔊
    m_muteButton->setToolTip(tr("Mute"));
    m_muteButton->setCursor(Qt::PointingHandCursor);
    m_muteButton->setStyleSheet(kPillButtonQss);
    connect(m_muteButton, &QPushButton::clicked, this, [this] {
        const bool muted = !m_audioOutput->isMuted();
        m_audioOutput->setMuted(muted);
        m_muteButton->setText(QString::fromUtf8(muted ? "\xF0\x9F\x94\x87"    // 🔇
                                                      : "\xF0\x9F\x94\x8A")); // 🔊
        m_muteButton->setToolTip(muted ? tr("Unmute") : tr("Mute"));
    });

    m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));  // ▶
    m_ui->playButton->setToolTip(tr("Play"));
    m_ui->playButton->setCursor(Qt::PointingHandCursor);
    m_ui->playButton->setStyleSheet(kPillButtonQss);

    m_motionLabel = new QLabel(tr("Motion"), this);
    m_motionLabel->setStyleSheet(kPillLabelQss);

    m_motionPill = new QFrame(this);
    m_motionPill->setObjectName("motionPill");
    m_motionPill->setStyleSheet("#motionPill{background:rgba(0,0,0,140);border-radius:15px;}");
    auto* pillLayout = new QHBoxLayout(m_motionPill);
    pillLayout->setContentsMargins(5, 4, 5, 4);
    pillLayout->setSpacing(4);
    pillLayout->addWidget(m_motionLabel);
    pillLayout->addWidget(m_ui->playButton);   // reparents into the pill
    pillLayout->addWidget(m_muteButton);
    m_motionPill->hide();

    // Panorama overlay (shown for equirectangular photos): the same pill as the
    // motion one, with a "360" button that asks the parent to open the sphere viewer.
    m_panoButton = new QPushButton(this);
    m_panoButton->setText("360");
    m_panoButton->setToolTip(tr("View panorama"));
    m_panoButton->setCursor(Qt::PointingHandCursor);
    m_panoButton->setStyleSheet(kPillButtonQss);
    connect(m_panoButton, &QPushButton::clicked, this, [this]{ emit panoramaRequested(); });

    m_panoLabel = new QLabel(tr("Panorama"), this);
    m_panoLabel->setStyleSheet(kPillLabelQss);

    m_panoPill = new QFrame(this);
    m_panoPill->setObjectName("panoPill");
    m_panoPill->setStyleSheet("#panoPill{background:rgba(0,0,0,140);border-radius:15px;}");
    auto* panoLayout = new QHBoxLayout(m_panoPill);
    panoLayout->setContentsMargins(5, 4, 5, 4);
    panoLayout->setSpacing(4);
    panoLayout->addWidget(m_panoLabel);
    panoLayout->addWidget(m_panoButton);
    m_panoPill->hide();


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
    m_zoom = 1.0;            // every new image starts fit-to-window
    m_dragging = false;
    m_hasVideo = false;
    unsetCursor();
    m_player->stop();
    m_videoWidget->hide();
    m_motionPill->hide();
    m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));  // ▶
    m_ui->playButton->setToolTip(tr("Play"));
    m_panoPill->hide();
    update();
}

void ImageRenderWidget::SetPlayButtonVisible(bool state)
{
    positionPlayButton();
    m_motionPill->setVisible(state);
    if (state)
        m_motionPill->raise();
}

void ImageRenderWidget::SetVideoManager(std::shared_ptr<QBuffer> videoBuff)
{
    QBuffer* btArray = new QBuffer(this);
    btArray->setData(videoBuff->data());
    m_player->setSourceDevice(btArray);
    m_hasVideo = true;
}

void ImageRenderWidget::togglePlayButton()
{
    if (m_hasVideo && !m_videoWidget->isVisible())   // only meaningful before playback
        m_motionPill->setVisible(!m_motionPill->isVisible());
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

// Rectangle the still is actually drawn into: the fit rect scaled by m_zoom and
// translated by the pan offset. At m_zoom == 1 this equals the fit rect.
QRectF ImageRenderWidget::displayRect() const
{
    const QRect fit = currentImageRect();
    if (!fit.isValid())
        return QRectF();
    return QRectF(m_imgTopLeft, QSizeF(fit.size()) * m_zoom);
}

// Keep the panned image sensible: fill the widget when larger than it, centre
// it when smaller (which is always the case at zoom 1).
void ImageRenderWidget::clampOffset()
{
    const QRect fit = currentImageRect();
    if (!fit.isValid())
        return;
    const QSizeF sz = QSizeF(fit.size()) * m_zoom;

    if (sz.width() <= width())
        m_imgTopLeft.setX((width() - sz.width()) / 2.0);
    else
        m_imgTopLeft.setX(qBound(width() - sz.width(), m_imgTopLeft.x(), 0.0));

    if (sz.height() <= height())
        m_imgTopLeft.setY((height() - sz.height()) / 2.0);
    else
        m_imgTopLeft.setY(qBound(height() - sz.height(), m_imgTopLeft.y(), 0.0));
}

void ImageRenderWidget::setZoom(double zoom, const QPointF& anchor)
{
    zoom = qBound(1.0, zoom, 8.0);
    const QRect fit = currentImageRect();
    if (!fit.isValid() || qFuzzyCompare(zoom, m_zoom))
        return;

    // Keep the image point under the anchor fixed while zooming.
    const QPointF fitPoint = (anchor - m_imgTopLeft) / m_zoom;
    m_zoom = zoom;
    m_imgTopLeft = anchor - fitPoint * m_zoom;
    clampOffset();
    setCursor(m_zoom > 1.0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    if (m_zoom <= 1.0)
        unsetCursor();
    update();
}

void ImageRenderWidget::positionPlayButton()
{
    // Both pills sit in the photo's top-left corner, inset a little. Only one is
    // ever visible (a photo is a motion clip or a panorama, not both).
    const QRect img = currentImageRect();
    if (img.isNull())
        return;
    for (QFrame* pill : { m_motionPill, m_panoPill })
    {
        pill->adjustSize();
        pill->move(img.left() + 12, img.top() + 12);
    }
}

void ImageRenderWidget::SetPanoramaButtonVisible(bool state)
{
    positionPlayButton();
    m_panoPill->setVisible(state);
    if (state)
        m_panoPill->raise();
}

void ImageRenderWidget::paintEvent(QPaintEvent* event)
{
    (void)event;
    if (!m_stillImage && !m_nextImage)
        return;

    clampOffset();
    const QRectF r = displayRect();
    if (r.isNull())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (m_nextImage)
        painter.drawPixmap(r, QPixmap::fromImage(*m_nextImage.get()), QRectF(m_nextImage->rect()));
    else
        painter.drawPixmap(r, *m_stillImage, QRectF(m_stillImage->rect()));
}

void ImageRenderWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Keep the video overlay aligned with the photo on every size change.
    // Doing this here (rather than in paintEvent) keeps the native video
    // surface in sync even while a video is playing and covering the widget.
    updateVideoGeometry();
    positionPlayButton();
}

void ImageRenderWidget::zoomIn()
{
    setZoom(m_zoom * 1.25, rect().center());
}

void ImageRenderWidget::zoomOut()
{
    setZoom(m_zoom / 1.25, rect().center());
}

void ImageRenderWidget::resetZoom()
{
    setZoom(1.0, rect().center());
    unsetCursor();
}

void ImageRenderWidget::playVideo()
{
    if (m_motionPill->isVisible())   // only when a clip is loaded
        on_playButton_clicked();
}

void ImageRenderWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (event->button() == Qt::LeftButton && m_zoom > 1.0)
    {
        m_dragging = true;
        m_dragLast = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageRenderWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging)
    {
        m_imgTopLeft += event->pos() - m_dragLast;
        m_dragLast = event->pos();
        clampOffset();
        update();
    }
}

void ImageRenderWidget::mouseReleaseEvent(QMouseEvent* event)
{
    (void)event;
    if (m_dragging)
    {
        m_dragging = false;
        setCursor(m_zoom > 1.0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        if (m_zoom <= 1.0)
            unsetCursor();
    }
}

void ImageRenderWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit backRequested();   // double-click returns to the folder content
}

void ImageRenderWidget::statusChanged(QMediaPlayer::MediaStatus status)
{
    positionPlayButton();
    // handle status message
    switch (status) {
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::LoadedMedia:
        // setStatusInfo(QString());
        m_videoWidget->hide();
        m_motionPill->show();
        m_motionPill->raise();
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
        m_videoWidget->hide();
        m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));  // ▶ (back to start)
        m_ui->playButton->setToolTip(tr("Play"));
        m_motionPill->show();
        m_motionPill->raise();
        break;
    case QMediaPlayer::InvalidMedia:
        // displayErrorMessage();
        break;
    }
}

void ImageRenderWidget::on_playButton_clicked()
{
    // The pill stays up during playback, so this button toggles play/pause.
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
    {
        m_player->pause();
        m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));      // ▶
        m_ui->playButton->setToolTip(tr("Play"));
        return;
    }

    updateVideoGeometry(); // ensure the overlay matches the photo before showing
    m_videoWidget->show();
    m_motionPill->raise(); // keep the controls above the video surface
    m_player->play();
    m_ui->playButton->setText(QString::fromUtf8("\xE2\x9D\x9A\xE2\x9D\x9A"));  // ❚❚
    m_ui->playButton->setToolTip(tr("Pause"));
}
