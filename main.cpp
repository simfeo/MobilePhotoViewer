#include "mainwindow.h"

#include <QStyleFactory>
#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>

int main(int argc, char* argv[])
{
    // Phone cameras produce very large stills (e.g. 108 MP = ~432 MB decoded).
    // Qt's default 128 MB QImageReader allocation cap rejects them, so lift it.
    QImageReader::setAllocationLimit(0);

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