#include "mainwindow.h"

#include <QStyleFactory>
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    MainWindow w;
    w.show();
    if (QCoreApplication::arguments().size() > 1)
    {
        w.SetImage(QCoreApplication::arguments()[1]);
    }
    return a.exec();
}