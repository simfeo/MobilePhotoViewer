#ifndef FORMLESS_H
#define FORMLESS_H

#include <QWidget>

namespace Ui {
class FormLess;
}

class FormLess : public QWidget
{
    Q_OBJECT

public:
    explicit FormLess(QWidget *parent = nullptr);
    ~FormLess();

private slots:
    void on_close_clicked();

    void on_maximize_clicked();

    void on_restore_clicked();

    void on_minimize_clicked();

private:
    Ui::FormLess *ui;
};

#endif // FORMLESS_H
