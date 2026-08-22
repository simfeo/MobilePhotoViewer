#include "imagerenderwidget.h"
#include "ui_imagerenderwidget.h"

#include <QPainter>

#include <QAudioFormat>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QAudioSink>
#include <QVideoSink>
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
    : QWidget(parent), m_ui(new Ui::ImageRenderWidget), m_stillImage(nullptr), m_nextImage(nullptr), m_player(nullptr), m_audioOutput(nullptr), m_videoSink(nullptr), m_showVideo(false), m_zoom(1.0), m_imgTopLeft(0, 0), m_dragging(false), m_hasVideo(false)
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
    m_muteButton->setCursor(Qt::PointingHandCursor);
    m_muteButton->setStyleSheet(kPillButtonQss);
    connect(m_muteButton, &QPushButton::clicked, this, [this]
            {
        const bool muted = !m_audioOutput->isMuted();
        m_audioOutput->setMuted(muted);
        m_muteButton->setText(QString::fromUtf8(muted ? "\xF0\x9F\x94\x87"    // 🔇
                                                      : "\xF0\x9F\x94\x8A")); // 🔊
        retranslateControls(); });

    m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));   // ▶
    m_ui->playButton->setCursor(Qt::PointingHandCursor);
    m_ui->playButton->setStyleSheet(kPillButtonQss);

    m_motionLabel = new QLabel(this);
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
    m_panoButton->setCursor(Qt::PointingHandCursor);
    m_panoButton->setStyleSheet(kPillButtonQss);
    connect(m_panoButton, &QPushButton::clicked, this, [this]
            { emit panoramaRequested(); });

    m_panoLabel = new QLabel(this);
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

    // Depth overlay (Samsung Live Focus shots): opens the parallax viewer.
    m_depthButton = new QPushButton(this);
    m_depthButton->setText("3D");
    m_depthButton->setCursor(Qt::PointingHandCursor);
    m_depthButton->setStyleSheet(kPillButtonQss);
    connect(m_depthButton, &QPushButton::clicked, this, [this]
            { emit depthRequested(); });

    m_depthLabel = new QLabel(this);
    m_depthLabel->setStyleSheet(kPillLabelQss);

    m_depthPill = new QFrame(this);
    m_depthPill->setObjectName("depthPill");
    m_depthPill->setStyleSheet("#depthPill{background:rgba(0,0,0,140);border-radius:15px;}");
    auto* depthLayout = new QHBoxLayout(m_depthPill);
    depthLayout->setContentsMargins(5, 4, 5, 4);
    depthLayout->setSpacing(4);
    depthLayout->addWidget(m_depthLabel);
    depthLayout->addWidget(m_depthButton);
    m_depthPill->hide();

    m_player      = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    // Take the decoded frames and paint them ourselves (see paintEvent) instead of
    // handing the player a QVideoWidget.
    m_videoSink = new QVideoSink(this);
    m_player->setVideoSink(m_videoSink);
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame)
            {
        if (!m_showVideo){
            return;
}
        m_frame = frame;
        // Sweep footage is drawn outside the still's rect, so the whole widget has
        // to be invalidated or the clip is clipped to the photo's shape.
        m_videoFillsStill ? update(currentImageRect()) : update(); });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &ImageRenderWidget::statusChanged);

    // Last: retranslateControls() reads the player/audio state, so it must run
    // once both exist.
    retranslateControls();
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
    m_zoom     = 1.0;   // every new image starts fit-to-window
    m_dragging = false;
    m_hasVideo = false;
    unsetCursor();
    m_player->stop();
    stopVideo();
    m_motionPill->hide();
    m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));   // ▶
    retranslateControls();
    m_panoPill->hide();
    m_depthPill->hide();
    update();
}

void ImageRenderWidget::SetPlayButtonVisible(bool state)
{
    m_motionPill->setVisible(state);
    positionPlayButton();
    if (state)
    {
        m_motionPill->raise();
    }
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
    if (m_hasVideo && !m_showVideo)   // only meaningful before playback
    {
        m_motionPill->setVisible(!m_motionPill->isVisible());
    }
}

// Rectangle the current image is drawn into: scaled to fit while keeping the
// aspect ratio, then centred. The video overlay is sized to this same rect.
QRect ImageRenderWidget::currentImageRect() const
{
    QSize src;
    if (m_nextImage)
    {
        src = m_nextImage->size();
    }
    else if (m_stillImage)
    {
        src = m_stillImage->size();
    }

    if (src.isEmpty())
    {
        return QRect();
    }

    QSize scaled   = src.scaled(size(), Qt::KeepAspectRatio);
    int   x_offset = (width() - scaled.width()) / 2;
    int   y_offset = (height() - scaled.height()) / 2;
    return QRect(x_offset, y_offset, scaled.width(), scaled.height());
}

// Rectangle the still is actually drawn into: the fit rect scaled by m_zoom and
// translated by the pan offset. At m_zoom == 1 this equals the fit rect.
QRectF ImageRenderWidget::displayRect() const
{
    const QRect fit = currentImageRect();
    if (!fit.isValid())
    {
        return QRectF();
    }
    return QRectF(m_imgTopLeft, QSizeF(fit.size()) * m_zoom);
}

// Keep the panned image sensible: fill the widget when larger than it, centre
// it when smaller (which is always the case at zoom 1).
void ImageRenderWidget::clampOffset()
{
    const QRect fit = currentImageRect();
    if (!fit.isValid())
    {
        return;
    }
    const QSizeF sz = QSizeF(fit.size()) * m_zoom;

    if (sz.width() <= width())
    {
        m_imgTopLeft.setX((width() - sz.width()) / 2.0);
    }
    else
    {
        m_imgTopLeft.setX(qBound(width() - sz.width(), m_imgTopLeft.x(), 0.0));
    }

    if (sz.height() <= height())
    {
        m_imgTopLeft.setY((height() - sz.height()) / 2.0);
    }
    else
    {
        m_imgTopLeft.setY(qBound(height() - sz.height(), m_imgTopLeft.y(), 0.0));
    }
}

void ImageRenderWidget::setZoom(double zoom, const QPointF& anchor)
{
    zoom            = qBound(1.0, zoom, 8.0);
    const QRect fit = currentImageRect();
    if (!fit.isValid() || qFuzzyCompare(zoom, m_zoom))
    {
        return;
    }

    // Keep the image point under the anchor fixed while zooming.
    const QPointF fitPoint = (anchor - m_imgTopLeft) / m_zoom;
    m_zoom                 = zoom;
    m_imgTopLeft           = anchor - fitPoint * m_zoom;
    clampOffset();
    setCursor(m_zoom > 1.0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    if (m_zoom <= 1.0)
    {
        unsetCursor();
    }
    update();
}

void ImageRenderWidget::stopVideo()
{
    // Drop the frame as well as the flag: keeping it would leave the last frame on
    // screen instead of the still, and hold its buffer alive for no reason.
    if (!m_showVideo)
    {
        return;
    }
    m_showVideo = false;
    m_frame     = QVideoFrame();
    update();
}

void ImageRenderWidget::retranslateControls()
{
    // Both tooltips depend on the current state, so they are derived here rather
    // than set once: this is also what runs on a live language change. A
    // changeEvent can reach us before the player is built, hence the guard.
    if (!m_player || !m_audioOutput)
    {
        return;
    }

    const bool playing = (m_player->playbackState() == QMediaPlayer::PlayingState);
    m_ui->playButton->setToolTip(playing ? tr("Pause") : tr("Play"));
    m_muteButton->setToolTip(m_audioOutput->isMuted() ? tr("Unmute") : tr("Mute"));
    m_panoButton->setToolTip(tr("View panorama"));
    m_depthButton->setToolTip(tr("View in 3D"));
    m_motionLabel->setText(tr("Motion"));
    m_panoLabel->setText(tr("Panorama"));
    m_depthLabel->setText(tr("Depth"));
    positionPlayButton();   // a translated label changes the pill's width
}

void ImageRenderWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateControls();
    }
    QWidget::changeEvent(event);
}

void ImageRenderWidget::positionPlayButton()
{
    // The pills sit in a row in the photo's top-left corner. A motion panorama is
    // both a clip and a panorama, so more than one can be up at a time.
    const QRect img = currentImageRect();
    if (img.isNull())
    {
        return;
    }
    int x = img.left() + 12;
    for (QFrame* pill : { m_motionPill, m_panoPill, m_depthPill })
    {
        pill->adjustSize();
        pill->move(x, img.top() + 12);
        if (pill->isVisible())
        {
            x += pill->width() + 8;
        }
    }
}

void ImageRenderWidget::SetVideoGeometry(bool fillsStill, int extraRotation)
{
    m_videoFillsStill    = fillsStill;
    m_videoExtraRotation = extraRotation;
}

void ImageRenderWidget::SetPanoramaButtonVisible(bool state)
{
    m_panoPill->setVisible(state);
    positionPlayButton();
    if (state)
    {
        m_panoPill->raise();
    }
}

void ImageRenderWidget::SetDepthButtonVisible(bool state)
{
    m_depthPill->setVisible(state);
    positionPlayButton();
    if (state)
    {
        m_depthPill->raise();
    }
}

void ImageRenderWidget::paintEvent(QPaintEvent* event)
{
    (void)event;
    if (!m_stillImage && !m_nextImage)
    {
        return;
    }

    clampOffset();
    const QRectF r = displayRect();
    if (r.isNull())
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // A playing clip replaces the still. It is drawn into the fit rect rather than
    // the zoomed one: the clip shares the photo's framing, and zoom/pan apply to
    // the still only (as they did when the video was a separate widget).
    if (m_showVideo && m_frame.isValid())
    {
        QVideoFrame::PaintOptions options;
        if (m_videoFillsStill)
        {
            // A motion-photo clip differs from the still by a sub-percent, so
            // filling the photo's rect matches it without a visible letterbox.
            options.aspectRatioMode = Qt::IgnoreAspectRatio;
            m_frame.paint(&painter, currentImageRect(), options);
            return;
        }

        // Sweep footage: fixed 16:9 that has nothing to do with the still's shape,
        // so fit it in the widget on its own terms.
        options.aspectRatioMode = Qt::KeepAspectRatio;
        QSize shown             = m_frame.size();
        if (m_videoExtraRotation % 180)
        {
            shown.transpose();
        }
        shown.scale(size(), Qt::KeepAspectRatio);
        if (m_videoExtraRotation % 180)
        {
            shown.transpose();   // back to the frame's own axes for painting
        }

        painter.save();
        painter.translate(rect().center());
        painter.rotate(m_videoExtraRotation);
        m_frame.paint(&painter, QRect(QPoint(-shown.width() / 2, -shown.height() / 2), shown), options);
        painter.restore();
        return;
    }

    if (m_nextImage)
    {
        painter.drawPixmap(r, QPixmap::fromImage(*m_nextImage.get()), QRectF(m_nextImage->rect()));
    }
    else
    {
        painter.drawPixmap(r, *m_stillImage, QRectF(m_stillImage->rect()));
    }
}

void ImageRenderWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // The clip is painted into currentImageRect(), which is derived from the
    // widget's size, so a resize needs no video-specific work beyond a repaint.
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
    {
        on_playButton_clicked();
    }
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
        {
            unsetCursor();
        }
    }
}

void ImageRenderWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit backRequested();   // double-click returns to the folder content
    }
}

void ImageRenderWidget::statusChanged(QMediaPlayer::MediaStatus status)
{
    positionPlayButton();
    // handle status message
    switch (status)
    {
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::LoadedMedia:
        // setStatusInfo(QString());
        stopVideo();
        m_motionPill->show();
        m_motionPill->raise();
        positionPlayButton();   // the row width changed now the pill is up
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
        stopVideo();
        m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));   // ▶ (back to start)
        retranslateControls();
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
        m_ui->playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));   // ▶
        retranslateControls();
        return;
    }

    m_showVideo = true;   // paintEvent draws frames from here on
    m_player->play();
    m_ui->playButton->setText(QString::fromUtf8("\xE2\x9D\x9A\xE2\x9D\x9A"));   // ❚❚
    retranslateControls();
    update();
}
