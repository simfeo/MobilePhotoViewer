#include "videowidget.h"
#include "ui_videowidget.h"

VideoWidget::VideoWidget(QWidget *parent) :
    QVideoWidget(parent),
    ui(new Ui::videowidget)
{
    ui->setupUi(this);
}

VideoWidget::~VideoWidget()
{
    delete ui;
}
