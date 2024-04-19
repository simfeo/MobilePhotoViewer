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

signals:
    void playVideoButtonClickedSignal();

private slots:
    void on_playButton_clicked();
    void statusChanged(QMediaPlayer::MediaStatus status);

private:
    void startVideoThread();
    void startAudioThread();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Ui::ImageRenderWidget*          m_ui;
    std::shared_ptr<QPixmap>        m_stillImage;
    std::shared_ptr<QImage>         m_nextImage;

    VideoWidget* m_videoWidget;
    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;

};

#endif // IMAGERENDERWIDGET_H
