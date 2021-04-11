#ifndef IMAGERENDERWIDGET_H
#define IMAGERENDERWIDGET_H

#include <QWidget>
#include <memory>

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
    void SetVideoManager(std::shared_ptr<VideoDecoder> videoDecoder);

signals:
    void playVideoButtonClickedSignal();

private slots:
    void on_playButton_clicked();

private:
    void startVideoThread();
    void startAudioThread();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Ui::ImageRenderWidget*          m_ui;
    std::shared_ptr<QPixmap>        m_stillImage;
    std::shared_ptr<VideoDecoder>   m_videoDecoder;
    std::shared_ptr<QImage>         m_nextImage;
    std::shared_ptr<std::thread>    m_videoUpdater;
    std::shared_ptr<std::thread>    m_audioUpdater;
};

#endif // IMAGERENDERWIDGET_H
