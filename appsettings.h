#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>
#include <QByteArray>

// Thin, typed wrapper over QSettings for the handful of persisted preferences.
// Keeps the key strings and defaults in one place so the settings dialog and the
// startup path can't drift apart. Storage location is set by the org/app name in
// main().
namespace AppSettings
{
    bool    useLastFolder();                 // start from the last folder we browsed
    void    setUseLastFolder(bool on);

    QString startFolder();                   // fixed start folder (default: home)
    void    setStartFolder(const QString& path);

    QString lastFolder();                    // last folder actually browsed
    void    setLastFolder(const QString& path);

    int     defaultThumbSize();              // thumbnail size at launch (px)
    void    setDefaultThumbSize(int px);

    // Folder to open at launch, honoring useLastFolder and falling back through
    // startFolder to the home directory when a path no longer exists.
    QString resolvedStartupFolder();

    // Main-window layout, persisted across launches. Geometry is the window's
    // size/position; state is the dock/toolbar arrangement (QMainWindow::saveState).
    QByteArray windowGeometry();
    void       setWindowGeometry(const QByteArray& data);
    void       clearWindowGeometry();       // for --reset-window

    QByteArray windowState();
    void       setWindowState(const QByteArray& data);
}

#endif // APPSETTINGS_H
