#include "frameless.h"
#include "ui_frameless.h"
#include "Widgets/titlewidget.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QCloseEvent>
#include <QScreen>

const int CONST_DRAG_BORDER_SIZE = 4;

FormLess::FormLess(QWidget *parent)
    : QWidget(parent)
    , mousePressed(false)
    , m_bDragTop (false)
    , m_bDragLeft (false)
    , m_bDragRight (false)
    , m_bDragBottom (false)
    , ui(new Ui::Frameless)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

#if defined(Q_OS_WIN)
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
#endif
    connect(ui->title, &TitleWidget::doubleClicked, this, &FormLess::on_maximize_clicked);
    connect(ui->title, &TitleWidget::parentEvent, this, &FormLess::childMouseEvent);
    ui->restore->hide();
    setMouseTracking(true);
    QApplication::instance()->installEventFilter(this);
}

FormLess::~FormLess()
{
    delete ui;
}

void FormLess::setTitleName(const QString& appname)
{
    ui->appName->setText(appname);
}

void FormLess::setIcon(const QPixmap &icon)
{
    ui->logo->setPixmap(icon);
}

void FormLess::on_close_clicked()
{
    close();
}

void FormLess::on_maximize_clicked()
{
    showMaximized();
    ui->restore->show();
    ui->maximize->hide();
    mousePressed = false;
}

void FormLess::on_restore_clicked()
{
    showNormal();
    ui->restore->hide();
    ui->maximize->show();
    mousePressed = false;
}

void FormLess::on_minimize_clicked()
{
    showMinimized();
    mousePressed = false;
}

void FormLess::childMouseEvent(QMouseEvent *event)
{
    if (isMaximized())
    {
        return;
    }

    // check mouse move event when mouse is moved on any object
    if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *pMouse = dynamic_cast<QMouseEvent *>(event);
        if (pMouse)
        {
            checkBorderDragging(pMouse);
        }
    }
    // press is triggered only on frame window
    else if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *pMouse = dynamic_cast<QMouseEvent *>(event);
        if (pMouse)
        {
            mousePressEvent(pMouse);
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        if (mousePressed)
        {
            QMouseEvent *pMouse = dynamic_cast<QMouseEvent *>(event);
            if (pMouse)
            {
                mouseReleaseEvent(pMouse);
            }
        }
    }
}

void FormLess::mousePressEvent(QMouseEvent *event)
{
    mousePressed = true;
    mousePos = event->globalPos();
    wndPos = pos();
    m_StartGeometry = this->geometry();


    QPoint globalMousePos = mapToGlobal(QPoint(event->x(), event->y()));

    if (leftBorderHit(globalMousePos) && topBorderHit(globalMousePos))
    {
        m_bDragTop = true;
        m_bDragLeft = true;
        setCursor(Qt::SizeFDiagCursor);
    }
    else if (rightBorderHit(globalMousePos) && topBorderHit(globalMousePos))
    {
        m_bDragRight = true;
        m_bDragTop = true;
        setCursor(Qt::SizeBDiagCursor);
    }
    else if (leftBorderHit(globalMousePos) && bottomBorderHit(globalMousePos))
    {
        m_bDragLeft = true;
        m_bDragBottom = true;
        setCursor(Qt::SizeBDiagCursor);
    }
    else if (rightBorderHit(globalMousePos) && bottomBorderHit(globalMousePos))
    {
        m_bDragRight = true;
        m_bDragBottom = true;
        setCursor(Qt::SizeFDiagCursor);
    }
    else
    {
        if (topBorderHit(globalMousePos))
        {
            m_bDragTop = true;
            setCursor(Qt::SizeVerCursor);
        }
        else if (leftBorderHit(globalMousePos))
        {
            m_bDragLeft = true;
            setCursor(Qt::SizeHorCursor);
        }
        else if (rightBorderHit(globalMousePos))
        {
            m_bDragRight = true;
            setCursor(Qt::SizeHorCursor);
        }
        else if (bottomBorderHit(globalMousePos))
        {
            m_bDragBottom = true;
            setCursor(Qt::SizeVerCursor);
        }
    }
}

void FormLess::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    mousePressed = false;
}

bool FormLess::eventFilter(QObject *obj, QEvent *event)
{
    if (isMaximized())
    {
        return QWidget::eventFilter(obj, event);
    }

    // check mouse move event when mouse is moved on any object
    if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *pMouse = dynamic_cast<QMouseEvent *>(event);
        if (pMouse)
        {
            checkBorderDragging(pMouse);
        }
    }
    // press is triggered only on frame window
    else if (event->type() == QEvent::MouseButtonPress && obj == this)
    {
        QMouseEvent *pMouse = dynamic_cast<QMouseEvent *>(event);
        if (pMouse)
        {
            mousePressEvent(pMouse);
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        if (mousePressed)
        {
            QMouseEvent *pMouse = dynamic_cast<QMouseEvent *>(event);
            if (pMouse)
            {
                mouseReleaseEvent(pMouse);
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void FormLess::checkBorderDragging(QMouseEvent *event)
{
    if (isMaximized()) {
        return;
    }

    QPoint globalMousePos = event->globalPos();
    if (mousePressed)
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        // available geometry excludes taskbar
        QRect availGeometry = screen->availableGeometry();
        int h = availGeometry.height();
        int w = availGeometry.width();
        QList<QScreen *> screenlist = screen->virtualSiblings();
        if (screenlist.contains(screen))
        {
            QSize sz = QApplication::desktop()->size();
            h = sz.height();
            w = sz.width();
        }

        // top right corner
        if (m_bDragTop && m_bDragRight)
        {
            int diff = globalMousePos.x() - (m_StartGeometry.x() + m_StartGeometry.width());
            int neww = m_StartGeometry.width() + diff;
            diff = globalMousePos.y() - m_StartGeometry.y();
            int newy = m_StartGeometry.y() + diff;
            if (neww > 0 && newy > 0 && newy < h - 50)
            {
                QRect newg = m_StartGeometry;
                newg.setWidth(neww);
                newg.setX(m_StartGeometry.x());
                newg.setY(newy);
                setGeometry(newg);
            }
        }
        // top left corner
        else if (m_bDragTop && m_bDragLeft)
        {
            int diff = globalMousePos.y() - m_StartGeometry.y();
            int newy = m_StartGeometry.y() + diff;
            diff = globalMousePos.x() - m_StartGeometry.x();
            int newx = m_StartGeometry.x() + diff;
            if (newy > 0 && newx > 0) {
                QRect newg = m_StartGeometry;
                newg.setY(newy);
                newg.setX(newx);
                setGeometry(newg);
            }
        }
        // bottom left corner
        else if (m_bDragBottom && m_bDragLeft)
        {
            int diff = globalMousePos.y() - (m_StartGeometry.y() + m_StartGeometry.height());
            int newh = m_StartGeometry.height() + diff;
            diff = globalMousePos.x() - m_StartGeometry.x();
            int newx = m_StartGeometry.x() + diff;
            if (newh > 0 && newx > 0) {
                QRect newg = m_StartGeometry;
                newg.setX(newx);
                newg.setHeight(newh);
                setGeometry(newg);
            }
        }
        // bottom right corner
        else if (m_bDragBottom && m_bDragRight)
        {
            int diff = globalMousePos.y() - (m_StartGeometry.y() + m_StartGeometry.height());
            int newh = m_StartGeometry.height() + diff;
            diff = globalMousePos.x() - (m_StartGeometry.x() + m_StartGeometry.width());
            int neww = m_StartGeometry.width() + diff;
            if (newh > 0 && neww > 0) {
                QRect newg = m_StartGeometry;
                newg.setHeight(newh);
                newg.setWidth(neww);
                setGeometry(newg);
            }
        }
        else if (m_bDragTop)
        {
            int diff = globalMousePos.y() - m_StartGeometry.y();
            int newy = m_StartGeometry.y() + diff;
            if (newy > 0 && newy < h - 50)
            {
                QRect newg = m_StartGeometry;
                newg.setY(newy);
                setGeometry(newg);
            }
        }
        else if (m_bDragLeft)
        {
            int diff = globalMousePos.x() - m_StartGeometry.x();
            int newx = m_StartGeometry.x() + diff;
            if (newx > 0 && newx < w - 50)
            {
                QRect newg = m_StartGeometry;
                newg.setX(newx);
                setGeometry(newg);
            }
        }
        else if (m_bDragRight)
        {
            int diff = globalMousePos.x() - (m_StartGeometry.x() + m_StartGeometry.width());
            int neww = m_StartGeometry.width() + diff;
            if (neww > 0) {
                QRect newg = m_StartGeometry;
                newg.setWidth(neww);
                newg.setX(m_StartGeometry.x());
                setGeometry(newg);
            }
        }
        else if (m_bDragBottom)
        {
            int diff = globalMousePos.y() - (m_StartGeometry.y() + m_StartGeometry.height());
            int newh = m_StartGeometry.height() + diff;
            if (newh > 0)
            {
                QRect newg = m_StartGeometry;
                newg.setHeight(newh);
                newg.setY(m_StartGeometry.y());
                setGeometry(newg);
            }
        }
    }
    else
    {
        // no mouse pressed
        if (leftBorderHit(globalMousePos) && topBorderHit(globalMousePos))
        {
            setCursor(Qt::SizeFDiagCursor);
        }
        else if (rightBorderHit(globalMousePos) && bottomBorderHit(globalMousePos))
        {
            setCursor(Qt::SizeFDiagCursor);
        }
        else if (rightBorderHit(globalMousePos) && topBorderHit(globalMousePos))
        {
            setCursor(Qt::SizeBDiagCursor);
        }
        else if (leftBorderHit(globalMousePos) && bottomBorderHit(globalMousePos))
        {
            setCursor(Qt::SizeBDiagCursor);
        }
        else
        {
            if (topBorderHit(globalMousePos))
            {
                setCursor(Qt::SizeVerCursor);
            }
            else if (leftBorderHit(globalMousePos))
            {
                setCursor(Qt::SizeHorCursor);
            }
            else if (rightBorderHit(globalMousePos))
            {
                setCursor(Qt::SizeHorCursor);
            }
            else if (bottomBorderHit(globalMousePos))
            {
                setCursor(Qt::SizeVerCursor);
            }
            else
            {
                m_bDragTop = false;
                m_bDragLeft = false;
                m_bDragRight = false;
                m_bDragBottom = false;
                setCursor(Qt::ArrowCursor);
            }
        }
    }
}

bool FormLess::leftBorderHit(const QPoint &pos)
{
    const QRect &rect = this->geometry();
    if (pos.x() >= rect.x() && pos.x() <= rect.x() + CONST_DRAG_BORDER_SIZE)
    {
        return true;
    }
    return false;
}

bool FormLess::rightBorderHit(const QPoint &pos)
{
    const QRect &rect = this->geometry();
    int tmp = rect.x() + rect.width();
    if (pos.x() <= tmp && pos.x() >= (tmp - CONST_DRAG_BORDER_SIZE))
    {
        return true;
    }
    return false;
}

bool FormLess::topBorderHit(const QPoint &pos)
{
    const QRect &rect = this->geometry();
    if (pos.y() >= rect.y() && pos.y() <= rect.y() + CONST_DRAG_BORDER_SIZE)
    {
        return true;
    }
    return false;
}

bool FormLess::bottomBorderHit(const QPoint &pos)
{
    const QRect &rect = this->geometry();
    int tmp = rect.y() + rect.height();
    if (pos.y() <= tmp && pos.y() >= (tmp - CONST_DRAG_BORDER_SIZE))
    {
        return true;
    }
    return false;
}
