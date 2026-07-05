#include "mainwindow.h"

#include <QStyleFactory>
#include <QApplication>
#include <QFileInfo>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    a.setWindowIcon(QIcon(":/appicon.png"));
    MainWindow w;
    w.show();
    if (QCoreApplication::arguments().size() > 1)
    {
        const QString arg = QCoreApplication::arguments()[1];
        if (QFileInfo(arg).isDir())
            w.OpenFolder(arg);
        else
            w.SetImage(arg);
    }
    return a.exec();
}