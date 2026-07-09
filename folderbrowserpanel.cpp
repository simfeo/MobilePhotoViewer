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
#include <QEvent>
#include <QWheelEvent>
#include <QNativeGestureEvent>
#include <QShortcut>

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
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setUniformItemSizes(true);
    m_grid->setMovement(QListView::Static);
    m_grid->setSpacing(6);
    // Single-line, elided captions ("long_name_1234...") keep every row the same
    // height so names line up in a clean grid (Explorer / ACDSee style).
    m_grid->setWordWrap(false);
    m_grid->setTextElideMode(Qt::ElideRight);

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

    // Thumbnail zoom: Ctrl+wheel / trackpad pinch (via the event filter), and the
    // +/- keys while the grid is focused.
    applyThumbSize();
    m_grid->viewport()->installEventFilter(this);

    const auto addZoomKey = [this](QKeySequence seq, int steps) {
        QShortcut* sc = new QShortcut(seq, m_grid);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, [this, steps] { zoomThumbnails(steps); });
    };
    addZoomKey(QKeySequence(Qt::Key_Plus),  +1);
    addZoomKey(QKeySequence(Qt::Key_Equal), +1);   // '+' key without Shift
    addZoomKey(QKeySequence(Qt::Key_Minus), -1);
}

QWidget* FolderBrowserPanel::folderTreeWidget() const
{
    return m_folderTree;
}

QWidget* FolderBrowserPanel::gridWidget() const
{
    return m_grid;
}

void FolderBrowserPanel::applyThumbSize()
{
    m_grid->setIconSize(QSize(m_thumbSize, m_thumbSize));
    // Leave room around the icon for spacing and the single-line caption.
    m_grid->setGridSize(QSize(m_thumbSize + 22, m_thumbSize + 22));
}

void FolderBrowserPanel::zoomThumbnails(int steps)
{
    const int size = qBound(kThumbMin, m_thumbSize + steps * kThumbStep, kThumbMax);
    if (size == m_thumbSize)
        return;
    m_thumbSize = size;
    applyThumbSize();
}

bool FolderBrowserPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_grid->viewport())
    {
        if (event->type() == QEvent::Wheel)
        {
            auto* we = static_cast<QWheelEvent*>(event);
            if (we->modifiers() & Qt::ControlModifier)
            {
                zoomThumbnails(we->angleDelta().y() > 0 ? +1 : -1);
                return true;   // consume: don't also scroll the list
            }
        }
        else if (event->type() == QEvent::NativeGesture)
        {
            auto* ng = static_cast<QNativeGestureEvent*>(event);
            if (ng->gestureType() == Qt::ZoomNativeGesture)
            {
                // macOS trackpad pinch: value() is a small per-event delta
                // (>0 = spread/zoom-in). Accumulate until it crosses one step.
                m_pinchAccum += ng->value();
                while (m_pinchAccum >=  0.10) { zoomThumbnails(+1); m_pinchAccum -= 0.10; }
                while (m_pinchAccum <= -0.10) { zoomThumbnails(-1); m_pinchAccum += 0.10; }
                return true;
            }
        }
    }
    return QObject::eventFilter(watched, event);
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
