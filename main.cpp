#include "mainwindow.h"
#include "appsettings.h"

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
    // Stable QSettings location (see SettingsDialog / AppSettings).
    QCoreApplication::setOrganizationName("MobilePhotoViewer");
    QCoreApplication::setApplicationName("MobilePhotoViewer");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    a.setWindowIcon(QIcon(":/appicon.png"));

    // Parse args: --reset-window discards the saved window size/position (the dock
    // layout is unaffected); the first non-option argument is a file or folder to
    // open. Do this before constructing the window, which restores the geometry.
    QString openPath;
    for (const QString& arg : QCoreApplication::arguments().mid(1))
    {
        if (arg == "--reset-window")
            AppSettings::clearWindowGeometry();
        else if (!arg.startsWith("--") && openPath.isEmpty())
            openPath = arg;
    }

    MainWindow w;
    w.show();
    if (!openPath.isEmpty())
    {
        if (QFileInfo(openPath).isDir())
            w.OpenFolder(openPath);
        else
            w.SetImage(openPath);
    }
    return a.exec();
}