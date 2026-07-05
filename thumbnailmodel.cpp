#include "thumbnailmodel.h"

#include <QThreadPool>
#include <QImageReader>
#include <QIcon>
#include <QFileInfo>
#include <QPainter>

ThumbnailModel::ThumbnailModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_pool(new QThreadPool(this))
{
    m_pool->setMaxThreadCount(4);

    // Neutral placeholder shown until the real thumbnail has been generated.
    m_placeholder = QPixmap(kThumb, kThumb);
    m_placeholder.fill(QColor(60, 60, 60));
    QPainter p(&m_placeholder);
    p.setPen(QColor(110, 110, 110));
    p.drawRect(0, 0, kThumb - 1, kThumb - 1);
}

void ThumbnailModel::setFiles(const QStringList& files)
{
    beginResetModel();
    m_files = files;
    m_cache.clear();
    m_pending.clear();
    endResetModel();
}

QString ThumbnailModel::filePath(const QModelIndex& index) const
{
    if (index.isValid() && index.row() < m_files.size())
        return m_files.at(index.row());
    return QString();
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

    switch (role)
    {
    case Qt::DisplayRole:
        return QFileInfo(path).fileName();
    case Qt::ToolTipRole:
        return path;
    case Qt::DecorationRole:
    {
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
        m_cache.insert(path, QPixmap::fromImage(image));

    const int row = m_files.indexOf(path);
    if (row >= 0)
    {
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { Qt::DecorationRole });
    }
}
