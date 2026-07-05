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
    void SetVideoManager(std::shared_ptr<QBuffer> videoBuff);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void playVideo();
    void togglePlayButton();

signals:
    void playVideoButtonClickedSignal();
    void backRequested();          // double-click in the viewer -> back to browsing

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

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
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
};

#endif // IMAGERENDERWIDGET_H
