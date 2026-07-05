#ifndef THUMBNAILMODEL_H
#define THUMBNAILMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QPixmap>
#include <QIcon>

class QThreadPool;
template<typename T> class QList;
class QFileInfo;

// List model that feeds a QListView (icon mode) with a folder's contents:
// sub-folders (folder icon) and images (thumbnail generated off the UI thread).
class ThumbnailModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ThumbnailModel(QObject* parent = nullptr);

    void    setEntries(const QList<QFileInfo>& entries);
    QString filePath(const QModelIndex& index) const;
    bool    isDir(const QModelIndex& index) const;

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private slots:
    void onThumbnailReady(const QString& path, const QImage& image);

private:
    void requestThumbnail(const QString& path) const;

    static const int kThumb = 128;

    QStringList                     m_files;
    QVector<bool>                   m_dirs;
    mutable QHash<QString, QPixmap> m_cache;
    mutable QSet<QString>           m_pending;
    QPixmap                         m_placeholder;
    QIcon                           m_folderIcon;
    QThreadPool*                    m_pool;
};

#endif // THUMBNAILMODEL_H
