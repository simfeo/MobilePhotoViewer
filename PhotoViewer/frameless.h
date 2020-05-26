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

private slots:
    void on_close_clicked();
    void on_maximize_clicked();
    void on_restore_clicked();
    void on_minimize_clicked();

private:
    Ui::Frameless *ui;
};

#endif // FORMLESS_H
