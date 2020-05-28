#include "titlewidget.h"
#include <QDebug>

TitleWidget::TitleWidget(QWidget* parent)
    : QWidget(parent)
{

}

TitleWidget::~TitleWidget()
{

}

void TitleWidget::mousePressEvent(QMouseEvent *event)
{
    mousePressed = true;
    mousePos = event->globalPos();

    QWidget *parent = parentWidget();
    if (parent)
    {
        parent = parent->parentWidget();
    }

    if (parent)
    {
        wndPos = parent->pos();
    }
}

void TitleWidget::mouseMoveEvent(QMouseEvent *event)
{
    QWidget *parent = parentWidget();
    if (parent)
    {
        parent = parent->parentWidget();
    }

    if (parent && mousePressed)
    {
        parent->move(wndPos + (event->globalPos() - mousePos));
    }
}

void TitleWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    mousePressed = false;
}

void TitleWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit doubleClicked();
}
