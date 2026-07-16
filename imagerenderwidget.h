#ifndef IMAGERENDERWIDGET_H
#define IMAGERENDERWIDGET_H

#include <QWidget>
#include <memory>

#include "videowidget.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QBuffer>

namespace Ui {
    class ImageRenderWidget;
}

class VideoDecoder;
class QPushButton;
class QFrame;
class QLabel;

class QImage;
namespace std
{
    class thread;
}

class ImageRenderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageRenderWidget(QWidget* parent = nullptr);
    ~ImageRenderWidget();

    void SetPixmap(const QPixmap& image);
    void SetPlayButtonVisible(bool state);
    void SetPanoramaButtonVisible(bool state);
    void SetVideoManager(std::shared_ptr<QBuffer> videoBuff);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void playVideo();
    void togglePlayButton();

signals:
    void playVideoButtonClickedSignal();
    void backRequested();          // double-click in the viewer -> back to browsing
    void panoramaRequested();      // the "360" overlay button was clicked

private slots:
    void on_playButton_clicked();
    void statusChanged(QMediaPlayer::MediaStatus status);

private:
    void startVideoThread();
    void startAudioThread();

    QRect  currentImageRect() const;   // fit-to-window rect (also used for the video)
    QRectF displayRect() const;        // zoomed/panned rect for the still image
    void   updateVideoGeometry();
    void   setZoom(double zoom, const QPointF& anchor);
    void   clampOffset();
    void   positionPlayButton();
    // Re-applies the pills' labels and tooltips. The play/mute tooltips depend on
    // the current state, so this is called on state changes as well as on a
    // language change.
    void   retranslateControls();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;   // QEvent::LanguageChange
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    Ui::ImageRenderWidget*          m_ui;
    std::shared_ptr<QPixmap>        m_stillImage;
    std::shared_ptr<QImage>         m_nextImage;

    VideoWidget* m_videoWidget;
    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;

    double  m_zoom;          // 1.0 == fit to window
    QPointF m_imgTopLeft;    // top-left of the displayed still, in widget coords
    bool    m_dragging;
    QPoint  m_dragLast;
    bool    m_hasVideo;      // a motion-photo clip is loaded

    // Overlay pills in the photo's top-left. A photo is either a motion clip or
    // a panorama, not both, so only one of these is ever visible.
    //
    // Motion: "Motion" label + play/pause (m_ui->playButton, reparented in) + mute.
    QFrame*      m_motionPill;
    QLabel*      m_motionLabel;
    QPushButton* m_muteButton;
    // Panorama: "Panorama" label + a "360" button that opens the sphere viewer.
    QFrame*      m_panoPill;
    QLabel*      m_panoLabel;
    QPushButton* m_panoButton;
};

#endif // IMAGERENDERWIDGET_H
