#include "appsettings.h"
#include "folderbrowserpanel.h"   // kThumbDefault / bounds

#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QObject>

namespace
{
constexpr auto kUseLastFolder = "startup/useLastFolder";
constexpr auto kStartFolder   = "startup/startFolder";
constexpr auto kLastFolder    = "startup/lastFolder";
constexpr auto kThumbSize     = "thumbnails/defaultSize";
constexpr auto kLanguage      = "ui/language";
constexpr auto kWinGeometry   = "window/geometry";
constexpr auto kWinState      = "window/state";

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

QString language()
{
    return QSettings().value(kLanguage, QString()).toString();
}

void setLanguage(const QString& code)
{
    QSettings().setValue(kLanguage, code);
}

QList<QPair<QString, QString>> availableLanguages()
{
    // Native names deliberately untranslated: a language is listed in its own
    // language, so it stays recognisable whatever the current UI language is.
    return {
        { QString(), QObject::tr("System default") },
        { "en",      QStringLiteral("English") },
        { "uk",      QStringLiteral("Українська") },
    };
}

QString resolvedStartupFolder()
{
    if (useLastFolder() && isDir(lastFolder()))
        return lastFolder();
    if (isDir(startFolder()))
        return startFolder();
    return QDir::homePath();
}

QByteArray windowGeometry()
{
    return QSettings().value(kWinGeometry).toByteArray();
}

void setWindowGeometry(const QByteArray& data)
{
    QSettings().setValue(kWinGeometry, data);
}

void clearWindowGeometry()
{
    QSettings().remove(kWinGeometry);
}

QByteArray windowState()
{
    return QSettings().value(kWinState).toByteArray();
}

void setWindowState(const QByteArray& data)
{
    QSettings().setValue(kWinState, data);
}
}   // namespace AppSettings
