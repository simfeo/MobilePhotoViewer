#include "frameless.h"
#include "ui_frameless.h"
#include "Widgets/titlewidget.h"

#include <QCloseEvent>

FormLess::FormLess(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Frameless)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

#if defined(Q_OS_WIN)
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
#endif
    connect(ui->title, &TitleWidget::doubleClicked, this, &FormLess::on_maximize_clicked);
    ui->restore->hide();
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
}

void FormLess::on_restore_clicked()
{
    showNormal();
    ui->restore->hide();
    ui->maximize->show();
}

void FormLess::on_minimize_clicked()
{
    showMinimized();
}
