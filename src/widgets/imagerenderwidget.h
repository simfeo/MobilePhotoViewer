#ifndef IMAGERENDERWIDGET_H
#define IMAGERENDERWIDGET_H

#include <QWidget>
#include <memory>

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QBuffer>
#include <QVideoFrame>

namespace Ui
{
    class ImageRenderWidget;
}

class VideoDecoder;
class QPushButton;
class QFrame;
class QLabel;
class QVideoSink;

class QImage;

class ImageRenderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageRenderWidget(QWidget* parent = nullptr);
    ~ImageRenderWidget();

    void SetPixmap(const QPixmap& image);
    // Swaps the displayed still only. SetPixmap() starts a new photo: it stops the
    // clip and clears the overlays, which a frame switch must not do.
    void SetStillImage(const QPixmap& image);
    void SetPlayButtonVisible(bool state);
    void SetPanoramaButtonVisible(bool state);
    void SetDepthButtonVisible(bool state);
    // Source-frame switcher: hidden when total < 2. 'current' is 1-based.
    void SetFrameSwitch(int current, int total);
    void SetVideoManager(std::shared_ptr<QBuffer> videoBuff);
    // Sweep footage does not share the still's framing: it is letterboxed at its
    // own aspect and turned by 'extraRotation' degrees to match the still.
    void SetVideoGeometry(bool fillsStill, int extraRotation);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void playVideo();
    void togglePlayButton();

signals:
    void playVideoButtonClickedSignal();
    void backRequested();          // double-click in the viewer -> back to browsing
    void panoramaRequested();      // the "360" overlay button was clicked
    void depthRequested();         // the "3D" overlay button was clicked
    void frameSwitchRequested();   // the source-frame pill was clicked

private slots:
    void on_playButton_clicked();
    void statusChanged(QMediaPlayer::MediaStatus status);

private:
    void startVideoThread();
    void startAudioThread();

    QRect  currentImageRect() const;   // fit-to-window rect (also used for the video)
    QRectF displayRect() const;        // zoomed/panned rect for the still image
    void   setZoom(double zoom, const QPointF& anchor);
    void   clampOffset();
    void   positionPlayButton();
    // Re-applies the pills' labels and tooltips. The play/mute tooltips depend on
    // the current state, so this is called on state changes as well as on a
    // language change.
    void retranslateControls();
    void stopVideo();   // back to showing the still

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;   // QEvent::LanguageChange
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    Ui::ImageRenderWidget*   m_ui;
    std::shared_ptr<QPixmap> m_stillImage;
    std::shared_ptr<QImage>  m_nextImage;

    // The clip is drawn straight into paintEvent() from the player's sink, rather
    // than through a QVideoWidget. Qt's QVideoWidget embeds a native QVideoWindow
    // via createWindowContainer(), and such a child window is not composited over a
    // full-screen top-level: the audio played on while the picture never appeared.
    // Painting the frames ourselves is the same path that already draws the still,
    // so it works in every window state, and keeps the overlay pills on top.
    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    QVideoSink*   m_videoSink;
    QVideoFrame   m_frame;       // most recent frame; invalid when nothing decoded yet
    bool          m_showVideo;   // paint m_frame instead of the still

    double  m_zoom;         // 1.0 == fit to window
    QPointF m_imgTopLeft;   // top-left of the displayed still, in widget coords
    bool    m_dragging;
    QPoint  m_dragLast;
    bool    m_hasVideo;   // a motion-photo clip is loaded
    bool    m_videoFillsStill    = true;
    int     m_videoExtraRotation = 0;

    // Overlay pills in the photo's top-left. Only one is ever visible: a photo is
    // a motion clip, a panorama or a depth shot, never two at once.
    //
    // Motion: "Motion" label + play/pause (m_ui->playButton, reparented in) + mute.
    QFrame*      m_motionPill;
    QLabel*      m_motionLabel;
    QPushButton* m_muteButton;
    // Panorama: "Panorama" label + a "360" button that opens the sphere viewer.
    QFrame*      m_panoPill;
    QLabel*      m_panoLabel;
    QPushButton* m_panoButton;
    // Depth: "Depth" label + a "3D" button that opens the parallax viewer.
    QFrame*      m_depthPill;
    QLabel*      m_depthLabel;
    QPushButton* m_depthButton;
    // Frames: sits on a second row, below the others, and cycles the source
    // frames DualShot/FocusShot stored alongside the still.
    QFrame*      m_framePill;
    QPushButton* m_frameButton;
    int          m_frameCurrent = 1;
    int          m_frameTotal   = 1;
};

#endif   // IMAGERENDERWIDGET_H
