#ifndef FORMLESS_H
#define FORMLESS_H

#include <QWidget>
#include <QPixmap>

namespace Ui {
class Frameless;
}

class FormLess : public QWidget
{
    Q_OBJECT

public:
    explicit FormLess(QWidget *parent = nullptr);
    ~FormLess();

public:
    void setTitleName(const QString& appname);
    void setIcon(const QPixmap& icon);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void checkBorderDragging(QMouseEvent *event);

private slots:
    void on_close_clicked();
    void on_maximize_clicked();
    void on_restore_clicked();
    void on_minimize_clicked();

private:
    bool leftBorderHit(const QPoint &pos);
    bool rightBorderHit(const QPoint &pos);
    bool topBorderHit(const QPoint &pos);
    bool bottomBorderHit(const QPoint &pos);

protected:
    bool mousePressed;
    bool m_bDragTop;
    bool m_bDragLeft;
    bool m_bDragRight;
    bool m_bDragBottom;

    QPoint mousePos;
    QPoint wndPos;
    QRect m_StartGeometry;

    Ui::Frameless *ui;
};

#endif // FORMLESS_H
