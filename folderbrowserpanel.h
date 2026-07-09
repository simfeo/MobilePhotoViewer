#ifndef FOLDERBROWSERPANEL_H
#define FOLDERBROWSERPANEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfoList>

class QWidget;
class QListView;
class QTreeView;
class QFileSystemModel;
class QModelIndex;
class QToolBar;
class QLineEdit;
class QAction;
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

    QWidget*  folderTreeWidget() const;   // for the Folders dock
    QWidget*  gridWidget()       const;   // for the central stack
    QToolBar* toolBar()          const;   // navigation + sort toolbar (top of window)

    const QStringList& supportedFormats() const { return m_supportedFormats; }

public slots:
    void openFolder(const QString& dirPath);   // select + load a folder in the tree
    void zoomThumbnails(int steps);            // +1 = larger, -1 = smaller (clamped)

signals:
    void folderChanged(const QString& dirPath);   // a folder was loaded into the grid
    void imageActivated(const QString& path);     // an image was double-clicked
    void previewRequested(const QString& path);   // grid selection changed (empty = none)

protected:
    // Ctrl+wheel (Windows/Linux) and trackpad pinch (macOS) zoom on the grid.
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onFolderSelected(const QModelIndex& index);
    void onThumbSelected(const QModelIndex& index);
    void onThumbActivated(const QModelIndex& index);
    void goBack();
    void goForward();
    void goUp();

private:
    // How the grid content is ordered. Folders are ALWAYS listed first and always
    // sorted by name; the key below only reorders the files among themselves.
    enum class SortKey { Name, DateCreated, DateModified, Size, Extension };

    void applyThumbSize();   // push m_thumbSize onto the grid's icon/grid size
    void buildToolBar();
    void reloadEntries();                              // re-sort + refill the grid
    QFileInfoList sortedEntries(const QString& dir) const;
    void updateNavActions();                           // enable/disable back/fwd/up

    static constexpr int kThumbMin  = 64;
    static constexpr int kThumbMax  = 128 * 3;   // 384
    static constexpr int kThumbStep = 32;

    QStringList        m_supportedFormats;
    QListView*         m_grid;
    ThumbnailModel*    m_thumbs;
    QTreeView*         m_folderTree;
    QFileSystemModel*  m_dirModel;
    int                m_thumbSize  = 128;   // current display size (icon side, px)
    double             m_pinchAccum = 0.0;   // accumulated macOS pinch magnitude

    // Navigation toolbar + history
    QToolBar*   m_toolBar     = nullptr;
    QAction*    m_backAct     = nullptr;
    QAction*    m_forwardAct  = nullptr;
    QAction*    m_upAct       = nullptr;
    QLineEdit*  m_locationEdit = nullptr;
    QStringList m_history;                 // visited folders, oldest -> newest
    int         m_histIndex   = -1;        // index into m_history of current folder
    bool        m_inHistoryNav = false;    // suppress recording during back/forward
    QString     m_currentPath;
    SortKey     m_sortKey       = SortKey::Name;
    bool        m_sortDescending = false;
};

#endif // FOLDERBROWSERPANEL_H
