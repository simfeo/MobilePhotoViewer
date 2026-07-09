#include "thumbnailmodel.h"

#include <QThreadPool>
#include <QImageReader>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFileIconProvider>
#include <QPainter>
#include <QMimeData>
#include <QUrl>
#include <QFile>

ThumbnailModel::ThumbnailModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_pool(new QThreadPool(this))
{
    m_pool->setMaxThreadCount(4);

    QFileIconProvider ip;
    m_folderIcon = ip.icon(QFileIconProvider::Folder);

    // Neutral placeholder shown until the real thumbnail has been generated.
    m_placeholder = QPixmap(kThumb, kThumb);
    m_placeholder.fill(QColor(60, 60, 60));
    QPainter p(&m_placeholder);
    p.setPen(QColor(110, 110, 110));
    p.drawRect(0, 0, kThumb - 1, kThumb - 1);
}

void ThumbnailModel::setEntries(const QList<QFileInfo>& entries)
{
    beginResetModel();
    m_files.clear();
    m_dirs.clear();
    m_cache.clear();
    m_pending.clear();
    for (const QFileInfo& fi : entries)
    {
        m_files.append(fi.absoluteFilePath());
        m_dirs.append(fi.isDir());
    }
    endResetModel();
}

QString ThumbnailModel::filePath(const QModelIndex& index) const
{
    if (index.isValid() && index.row() < m_files.size())
        return m_files.at(index.row());
    return QString();
}

bool ThumbnailModel::isDir(const QModelIndex& index) const
{
    return index.isValid() && index.row() < m_dirs.size() && m_dirs.at(index.row());
}

int ThumbnailModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_files.size();
}

QVariant ThumbnailModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_files.size())
        return QVariant();

    const QString path = m_files.at(index.row());
    const bool dir = m_dirs.at(index.row());

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::EditRole:                         // prefill the rename editor
        return QFileInfo(path).fileName();
    case Qt::ToolTipRole:
        return path;
    case Qt::DecorationRole:
    {
        if (dir)
            return m_folderIcon;
        auto it = m_cache.constFind(path);
        if (it != m_cache.constEnd())
            return QIcon(it.value());
        requestThumbnail(path);
        return QIcon(m_placeholder);
    }
    default:
        return QVariant();
    }
}

Qt::ItemFlags ThumbnailModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractListModel::flags(index);
    if (index.isValid())
    {
        f |= Qt::ItemIsEditable;   // rename (files and folders)
        // Only images are draggable; folders are for navigation, not drag-out.
        if (index.row() < m_dirs.size() && !m_dirs.at(index.row()))
            f |= Qt::ItemIsDragEnabled;
    }
    return f;
}

bool ThumbnailModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_files.size())
        return false;

    const QString newName = value.toString().trimmed();
    const QString oldPath = m_files.at(index.row());
    const QFileInfo fi(oldPath);
    if (newName.isEmpty() || newName == fi.fileName())
        return false;

    const QString newPath = fi.absolutePath() + '/' + newName;
    if (QFile::exists(newPath) || !QFile::rename(oldPath, newPath))
        return false;   // refuse to clobber an existing file / failed rename

    m_files[index.row()] = newPath;
    if (m_cache.contains(oldPath))
        m_cache.insert(newPath, m_cache.take(oldPath));   // keep the thumbnail
    emit dataChanged(index, index,
                     { Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::DecorationRole });
    return true;
}

QStringList ThumbnailModel::mimeTypes() const
{
    return { QStringLiteral("text/uri-list") };
}

QMimeData* ThumbnailModel::mimeData(const QModelIndexList& indexes) const
{
    QList<QUrl> urls;
    for (const QModelIndex& index : indexes)
    {
        const int row = index.row();
        if (!index.isValid() || row >= m_files.size())
            continue;
        if (row < m_dirs.size() && m_dirs.at(row))
            continue;   // skip folders -- images only
        urls.append(QUrl::fromLocalFile(m_files.at(row)));
    }
    if (urls.isEmpty())
        return nullptr;

    QMimeData* mime = new QMimeData;
    mime->setUrls(urls);   // text/uri-list -> Explorer copy, Telegram/WhatsApp share
    return mime;
}

Qt::DropActions ThumbnailModel::supportedDragActions() const
{
    return Qt::CopyAction;   // drag out = copy/share, never move the originals
}

void ThumbnailModel::requestThumbnail(const QString& path) const
{
    if (m_pending.contains(path))
        return;
    m_pending.insert(path);

    ThumbnailModel* self = const_cast<ThumbnailModel*>(this);
    const int box = kThumb;
    m_pool->start([self, path, box]() {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QSize s = reader.size();
        if (s.isValid())
        {
            s.scale(box, box, Qt::KeepAspectRatio);
            reader.setScaledSize(s);
        }
        QImage img = reader.read();
        QMetaObject::invokeMethod(self, "onThumbnailReady", Qt::QueuedConnection,
                                  Q_ARG(QString, path), Q_ARG(QImage, img));
    });
}

void ThumbnailModel::onThumbnailReady(const QString& path, const QImage& image)
{
    m_pending.remove(path);
    if (!image.isNull())
    {
        // Centre the (aspect-scaled) thumbnail on a fixed square canvas so every
        // item's decoration is the same size. Otherwise Qt places the caption
        // right under the actual pixmap, leaving names misaligned row to row.
        QPixmap canvas(kThumb, kThumb);
        canvas.fill(Qt::transparent);
        const QPixmap thumb = QPixmap::fromImage(image);
        QPainter p(&canvas);
        p.drawPixmap((kThumb - thumb.width()) / 2, (kThumb - thumb.height()) / 2, thumb);
        p.end();
        m_cache.insert(path, canvas);
    }

    const int row = m_files.indexOf(path);
    if (row >= 0)
    {
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { Qt::DecorationRole });
    }
}
