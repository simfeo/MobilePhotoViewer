#ifndef FOLDERBROWSERPANEL_H
#define FOLDERBROWSERPANEL_H

#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;
class QListView;
class QTreeView;
class QFileSystemModel;
class QModelIndex;
class ThumbnailModel;

// Owns the folder tree + thumbnail grid and the navigation between them. The tree
// lives in the Folders dock and the grid in the central stack, so the panel hands
// both widgets to the host (MainWindow) to place, and reports user actions via
// signals rather than reaching back into the window.
class FolderBrowserPanel : public QObject
{
    Q_OBJECT
public:
    explicit FolderBrowserPanel(QObject* parent = nullptr);

    QWidget* folderTreeWidget() const;   // for the Folders dock
    QWidget* gridWidget()       const;   // for the central stack

    const QStringList& supportedFormats() const { return m_supportedFormats; }

public slots:
    void openFolder(const QString& dirPath);   // select + load a folder in the tree

signals:
    void folderChanged(const QString& dirPath);   // a folder was loaded into the grid
    void imageActivated(const QString& path);     // an image was double-clicked
    void previewRequested(const QString& path);   // grid selection changed (empty = none)

private slots:
    void onFolderSelected(const QModelIndex& index);
    void onThumbSelected(const QModelIndex& index);
    void onThumbActivated(const QModelIndex& index);

private:
    QStringList        m_supportedFormats;
    QListView*         m_grid;
    ThumbnailModel*    m_thumbs;
    QTreeView*         m_folderTree;
    QFileSystemModel*  m_dirModel;
};

#endif // FOLDERBROWSERPANEL_H
