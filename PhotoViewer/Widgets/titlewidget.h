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
    void parentEvent(QMouseEvent* event);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

protected:
    bool   m_mousePressed;
    QPoint mousePos;
    QPoint wndPos;
};

#endif // TITLEWIDGET_H
