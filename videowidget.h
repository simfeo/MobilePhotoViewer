#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QVideoWidget>

namespace Ui {
class videowidget;
}

class VideoWidget : public QVideoWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

private:
    Ui::videowidget *ui;
};

#endif // VIDEOWIDGET_H
