#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H

#include <QWidget>
#include <QMouseEvent>

class TitleWidget : public QWidget
{
    Q_OBJECT
public:
    TitleWidget(QWidget* parent = nullptr);
    ~TitleWidget();
signals:
 void doubleClicked();

protected:
 void mousePressEvent(QMouseEvent *event);
 void mouseMoveEvent(QMouseEvent *event);
 void mouseReleaseEvent(QMouseEvent *event);
 void mouseDoubleClickEvent(QMouseEvent *event);

protected:
 QPoint mousePos;
 QPoint wndPos;
 bool mousePressed;
};

#endif // TITLEWIDGET_H
