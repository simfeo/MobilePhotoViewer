#include "appsettings.h"
#include "folderbrowserpanel.h"   // kThumbDefault / bounds

#include <QSettings>
#include <QDir>
#include <QFileInfo>

namespace
{
constexpr auto kUseLastFolder = "startup/useLastFolder";
constexpr auto kStartFolder   = "startup/startFolder";
constexpr auto kLastFolder    = "startup/lastFolder";
constexpr auto kThumbSize     = "thumbnails/defaultSize";

bool isDir(const QString& path)
{
    return !path.isEmpty() && QFileInfo(path).isDir();
}
}   // namespace

namespace AppSettings
{
bool useLastFolder()
{
    return QSettings().value(kUseLastFolder, false).toBool();
}

void setUseLastFolder(bool on)
{
    QSettings().setValue(kUseLastFolder, on);
}

QString startFolder()
{
    return QSettings().value(kStartFolder, QDir::homePath()).toString();
}

void setStartFolder(const QString& path)
{
    QSettings().setValue(kStartFolder, path);
}

QString lastFolder()
{
    return QSettings().value(kLastFolder).toString();
}

void setLastFolder(const QString& path)
{
    QSettings().setValue(kLastFolder, path);
}

int defaultThumbSize()
{
    const int px = QSettings().value(kThumbSize, FolderBrowserPanel::kThumbDefault).toInt();
    return qBound(FolderBrowserPanel::kThumbMin, px, FolderBrowserPanel::kThumbMax);
}

void setDefaultThumbSize(int px)
{
    QSettings().setValue(kThumbSize, px);
}

QString resolvedStartupFolder()
{
    if (useLastFolder() && isDir(lastFolder()))
        return lastFolder();
    if (isDir(startFolder()))
        return startFolder();
    return QDir::homePath();
}
}   // namespace AppSettings
