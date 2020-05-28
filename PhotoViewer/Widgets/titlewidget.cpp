#include "titlewidget.h"
#include <QDebug>

TitleWidget::TitleWidget(QWidget* parent)
    : QWidget(parent)
    , m_mousePressed(false)
{

}

TitleWidget::~TitleWidget()
{

}

void TitleWidget::mousePressEvent(QMouseEvent *event)
{
    if (cursor() != Qt::ArrowCursor)
    {
        emit parentEvent(event);
        return;
    }

    m_mousePressed = true;
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
    if (cursor() != Qt::ArrowCursor)
    {
        emit parentEvent(event);
        return;
    }

    QWidget *parent = parentWidget();
    if (parent)
    {
        parent = parent->parentWidget();
    }

    if (parent && m_mousePressed)
    {
        parent->move(wndPos + (event->globalPos() - mousePos));
    }
}

void TitleWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (cursor() != Qt::ArrowCursor)
    {
        emit parentEvent(event);
        return;
    }
    m_mousePressed = false;
}

void TitleWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit doubleClicked();
}
