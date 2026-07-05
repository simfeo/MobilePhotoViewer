#ifndef THUMBNAILMODEL_H
#define THUMBNAILMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QHash>
#include <QSet>
#include <QPixmap>

class QThreadPool;

// List model that feeds a QListView (icon mode) with image thumbnails, generated
// off the UI thread and cached. Rows are absolute file paths.
class ThumbnailModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ThumbnailModel(QObject* parent = nullptr);

    void    setFiles(const QStringList& files);
    QString filePath(const QModelIndex& index) const;

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private slots:
    void onThumbnailReady(const QString& path, const QImage& image);

private:
    void requestThumbnail(const QString& path) const;

    static const int kThumb = 128;

    QStringList                    m_files;
    mutable QHash<QString, QPixmap> m_cache;
    mutable QSet<QString>          m_pending;
    QPixmap                        m_placeholder;
    QThreadPool*                   m_pool;
};

#endif // THUMBNAILMODEL_H
