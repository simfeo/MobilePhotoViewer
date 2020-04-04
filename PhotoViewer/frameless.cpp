#include "frameless.h"
#include "ui_formless.h"

#include <QCloseEvent>

FormLess::FormLess(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormLess)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

#if defined(Q_OS_WIN)
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
#endif
}

FormLess::~FormLess()
{
    delete ui;
}

void FormLess::on_close_clicked()
{
    close();
}

void FormLess::on_maximize_clicked()
{
   showMaximized();
}

void FormLess::on_restore_clicked()
{
    showNormal();
}

void FormLess::on_minimize_clicked()
{
    showMinimized();
}
