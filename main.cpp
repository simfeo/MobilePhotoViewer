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


// d:\_pictures\DCIMS10\2020-03-22\20200318_063552.jpg
// d:\_pictures\DCIMS10\2021-02-21\20210126_120204.jpg
// d:\_pictures\DCIMS10\2021-02-21\20210119_112641.jpg
// d:\_pictures\DCIMS10\2021-02-21\20210124_000908.jpg
