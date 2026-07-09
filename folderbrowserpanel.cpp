#include "folderbrowserpanel.h"
#include "explorerselectiondelegate.h"
#include "thumbnailmodel.h"

#include <QListView>
#include <QTreeView>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QImageReader>
#include <QFileInfoList>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QEvent>
#include <QWheelEvent>
#include <QNativeGestureEvent>
#include <QShortcut>
#include <QToolBar>
#include <QToolButton>
#include <QLineEdit>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QPolygonF>
#include <QPalette>

#include <algorithm>

namespace {
// The platform's standard arrow icons are nearly black on a dark theme, so draw
// our own filled triangles in the toolbar's (palette-derived) text colour.
QIcon makeArrowIcon(Qt::ArrowType dir, const QColor& color, int px = 18)
{
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    const qreal m = px * 0.30;   // inset from the edges
    QPolygonF tri;
    switch (dir)
    {
    case Qt::LeftArrow:  tri << QPointF(px - m, m) << QPointF(m, px / 2.0) << QPointF(px - m, px - m); break;
    case Qt::RightArrow: tri << QPointF(m, m) << QPointF(px - m, px / 2.0) << QPointF(m, px - m); break;
    case Qt::UpArrow:    tri << QPointF(m, px - m) << QPointF(px / 2.0, m) << QPointF(px - m, px - m); break;
    default: break;
    }
    p.drawPolygon(tri);
    return QIcon(pm);
}
}   // namespace

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

    buildToolBar();
    updateNavActions();
}

QWidget* FolderBrowserPanel::folderTreeWidget() const
{
    return m_folderTree;
}

QWidget* FolderBrowserPanel::gridWidget() const
{
    return m_grid;
}

QToolBar* FolderBrowserPanel::toolBar() const
{
    return m_toolBar;
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

void FolderBrowserPanel::buildToolBar()
{
    m_toolBar = new QToolBar(tr("Navigation"));
    m_toolBar->setMovable(false);
    const QColor fg = m_toolBar->palette().color(QPalette::ButtonText);

    m_backAct = m_toolBar->addAction(makeArrowIcon(Qt::LeftArrow, fg), tr("Back"));
    m_backAct->setShortcut(QKeySequence::Back);
    connect(m_backAct, &QAction::triggered, this, &FolderBrowserPanel::goBack);

    m_forwardAct = m_toolBar->addAction(makeArrowIcon(Qt::RightArrow, fg), tr("Forward"));
    m_forwardAct->setShortcut(QKeySequence::Forward);
    connect(m_forwardAct, &QAction::triggered, this, &FolderBrowserPanel::goForward);

    m_upAct = m_toolBar->addAction(makeArrowIcon(Qt::UpArrow, fg), tr("Up"));
    connect(m_upAct, &QAction::triggered, this, &FolderBrowserPanel::goUp);

    m_toolBar->addSeparator();

    // Current location: shows the path, and typing one + Enter navigates to it.
    m_locationEdit = new QLineEdit;
    m_locationEdit->setClearButtonEnabled(true);
    m_locationEdit->setPlaceholderText(tr("Location"));
    connect(m_locationEdit, &QLineEdit::returnPressed, this,
            [this] { openFolder(m_locationEdit->text().trimmed()); });
    m_toolBar->addWidget(m_locationEdit);

    // Sort menu: an exclusive key, plus an exclusive ascending/descending toggle.
    QToolButton* sortBtn = new QToolButton;
    sortBtn->setText(tr("Sort"));
    sortBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    sortBtn->setPopupMode(QToolButton::InstantPopup);

    QMenu* sortMenu = new QMenu(sortBtn);
    QActionGroup* keyGroup = new QActionGroup(this);
    keyGroup->setExclusive(true);
    const auto addSort = [&](const QString& text, SortKey key) {
        QAction* a = sortMenu->addAction(text);
        a->setCheckable(true);
        a->setChecked(key == m_sortKey);
        keyGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, key] { m_sortKey = key; reloadEntries(); });
    };
    addSort(tr("Name"),          SortKey::Name);
    addSort(tr("Date Created"),  SortKey::DateCreated);
    addSort(tr("Date Modified"), SortKey::DateModified);
    addSort(tr("Size"),          SortKey::Size);
    addSort(tr("Extension"),     SortKey::Extension);

    sortMenu->addSeparator();
    QActionGroup* dirGroup = new QActionGroup(this);
    dirGroup->setExclusive(true);
    const auto addDir = [&](const QString& text, bool descending) {
        QAction* a = sortMenu->addAction(text);
        a->setCheckable(true);
        a->setChecked(descending == m_sortDescending);
        dirGroup->addAction(a);
        connect(a, &QAction::triggered, this,
                [this, descending] { m_sortDescending = descending; reloadEntries(); });
    };
    addDir(tr("Ascending"),  false);
    addDir(tr("Descending"), true);

    sortBtn->setMenu(sortMenu);
    m_toolBar->addWidget(sortBtn);
}

void FolderBrowserPanel::reloadEntries()
{
    if (m_currentPath.isEmpty())
        return;
    m_thumbs->setEntries(sortedEntries(m_currentPath));
}

QFileInfoList FolderBrowserPanel::sortedEntries(const QString& dir) const
{
    QDir d(dir);
    QFileInfoList folders = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList files   = d.entryInfoList(m_supportedFormats, QDir::Files);

    const auto byName = [](const QFileInfo& a, const QFileInfo& b) {
        return QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive) < 0;
    };

    // Folders are always listed first and always ordered by name.
    std::sort(folders.begin(), folders.end(), byName);

    const SortKey key = m_sortKey;
    std::sort(files.begin(), files.end(), [key, &byName](const QFileInfo& a, const QFileInfo& b) {
        switch (key)
        {
        case SortKey::DateCreated:
            if (a.birthTime() != b.birthTime())       return a.birthTime() < b.birthTime();
            break;
        case SortKey::DateModified:
            if (a.lastModified() != b.lastModified()) return a.lastModified() < b.lastModified();
            break;
        case SortKey::Size:
            if (a.size() != b.size())                 return a.size() < b.size();
            break;
        case SortKey::Extension: {
            const int c = QString::compare(a.suffix(), b.suffix(), Qt::CaseInsensitive);
            if (c != 0)                               return c < 0;
            break;   // same extension -> fall through to name
        }
        case SortKey::Name:
            break;
        }
        return byName(a, b);   // Name mode, and tie-breaker for every other key
    });

    // Descending reverses both lists but keeps folders grouped ahead of files.
    if (m_sortDescending)
    {
        std::reverse(folders.begin(), folders.end());
        std::reverse(files.begin(), files.end());
    }

    return folders + files;
}

void FolderBrowserPanel::updateNavActions()
{
    if (m_backAct)
        m_backAct->setEnabled(m_histIndex > 0);
    if (m_forwardAct)
        m_forwardAct->setEnabled(m_histIndex >= 0 && m_histIndex < m_history.size() - 1);
    if (m_upAct)
    {
        QDir d(m_currentPath);
        m_upAct->setEnabled(!m_currentPath.isEmpty() && d.cdUp());
    }
}

void FolderBrowserPanel::goBack()
{
    if (m_histIndex <= 0)
        return;
    --m_histIndex;
    m_inHistoryNav = true;
    openFolder(m_history.at(m_histIndex));   // selects in tree -> onFolderSelected
    m_inHistoryNav = false;
}

void FolderBrowserPanel::goForward()
{
    if (m_histIndex < 0 || m_histIndex >= m_history.size() - 1)
        return;
    ++m_histIndex;
    m_inHistoryNav = true;
    openFolder(m_history.at(m_histIndex));
    m_inHistoryNav = false;
}

void FolderBrowserPanel::goUp()
{
    if (m_currentPath.isEmpty())
        return;
    QDir d(m_currentPath);
    if (d.cdUp())
        openFolder(d.absolutePath());   // ordinary navigation -> records history
}

void FolderBrowserPanel::onFolderSelected(const QModelIndex& index)
{
    const QString dir = m_dirModel->filePath(index);
    if (dir.isEmpty())
        return;

    m_currentPath = dir;
    reloadEntries();                       // fill the grid (folders first, sorted)

    if (m_locationEdit)
        m_locationEdit->setText(dir);

    // Record the visit unless we got here by pressing Back/Forward.
    if (!m_inHistoryNav)
    {
        if (m_histIndex < m_history.size() - 1)
            m_history.erase(m_history.begin() + m_histIndex + 1, m_history.end());
        if (m_history.isEmpty() || m_history.last() != dir)
            m_history.append(dir);
        m_histIndex = m_history.size() - 1;
    }
    updateNavActions();

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
