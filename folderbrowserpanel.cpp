#include "folderbrowserpanel.h"
#include "explorerselectiondelegate.h"
#include "thumbnailmodel.h"

#include <QListView>
#include <QTreeView>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QImageReader>
#include <QFileInfoList>
#include <QDir>

FolderBrowserPanel::FolderBrowserPanel(QObject* parent)
    : QObject(parent)
{
    for (auto& i : QImageReader::supportedImageFormats())
        m_supportedFormats.append("*." + i);
    m_supportedFormats.append("*.heic");
    m_supportedFormats.append("*.heif");

    // Thumbnail grid (icon mode, like a file browser's content pane).
    m_thumbs = new ThumbnailModel(this);
    m_grid = new QListView;
    m_grid->setModel(m_thumbs);
    m_grid->setItemDelegate(new ExplorerSelectionDelegate(m_grid));
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setIconSize(QSize(128, 128));
    m_grid->setGridSize(QSize(150, 150));
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setUniformItemSizes(true);
    m_grid->setMovement(QListView::Static);
    m_grid->setSpacing(6);
    m_grid->setWordWrap(true);

    // Explorer-style selection: Ctrl/Shift extend, and a rubber-band rectangle
    // selects a range by mouse.
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setSelectionRectVisible(true);

    // Drag the selected images out of the app (copy to a folder, share to other
    // apps). DragOnly: the grid initiates drags but doesn't accept internal drops.
    m_grid->setDragEnabled(true);
    m_grid->setDragDropMode(QAbstractItemView::DragOnly);
    m_grid->setDefaultDropAction(Qt::CopyAction);

    // Folder tree backed by the filesystem.
    m_dirModel = new QFileSystemModel(this);
    m_dirModel->setFilter(QDir::Dirs | QDir::Drives | QDir::NoDotAndDotDot);
    m_dirModel->setRootPath("");
    m_folderTree = new QTreeView;
    m_folderTree->setModel(m_dirModel);
    m_folderTree->setItemDelegate(new ExplorerSelectionDelegate(m_folderTree));
    m_folderTree->setHeaderHidden(true);
    for (int c = 1; c < m_dirModel->columnCount(); ++c)
        m_folderTree->setColumnHidden(c, true);

    connect(m_folderTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &FolderBrowserPanel::onFolderSelected);
    connect(m_grid->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &FolderBrowserPanel::onThumbSelected);
    connect(m_grid, &QListView::doubleClicked, this, &FolderBrowserPanel::onThumbActivated);
}

QWidget* FolderBrowserPanel::folderTreeWidget() const
{
    return m_folderTree;
}

QWidget* FolderBrowserPanel::gridWidget() const
{
    return m_grid;
}

void FolderBrowserPanel::openFolder(const QString& dirPath)
{
    if (dirPath.isEmpty())
        return;
    // Selecting the folder in the tree drives onFolderSelected(), which loads it.
    const QModelIndex idx = m_dirModel->index(dirPath);
    if (idx.isValid())
    {
        m_folderTree->setCurrentIndex(idx);
        m_folderTree->scrollTo(idx);
        m_folderTree->expand(idx);
    }
}

void FolderBrowserPanel::onFolderSelected(const QModelIndex& index)
{
    const QString dir = m_dirModel->filePath(index);
    if (dir.isEmpty())
        return;

    // Show the folder's content: sub-folders first, then supported images -- so
    // the grid works as a file browser, not only a picture list.
    QDir d(dir);
    QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    entries += d.entryInfoList(m_supportedFormats, QDir::Files, QDir::Name);

    m_thumbs->setEntries(entries);
    emit folderChanged(dir);
}

void FolderBrowserPanel::onThumbSelected(const QModelIndex& index)
{
    if (!m_thumbs->isDir(index))
        emit previewRequested(m_thumbs->filePath(index));
    else
        emit previewRequested(QString());
}

void FolderBrowserPanel::onThumbActivated(const QModelIndex& index)
{
    const QString path = m_thumbs->filePath(index);
    if (path.isEmpty())
        return;
    if (m_thumbs->isDir(index))
        openFolder(path);           // navigate into the sub-folder
    else
        emit imageActivated(path);  // MainWindow opens it in the viewer
}
