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

private:
    void applyThumbSize();   // push m_thumbSize onto the grid's icon/grid size

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
};

#endif // FOLDERBROWSERPANEL_H
