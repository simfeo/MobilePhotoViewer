#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <Tools/imagewithvideobase.h>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QDropEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QWheelEvent;
class QKeyEvent;
class QStackedWidget;
class QListView;
class QTreeView;
class QFileSystemModel;
class QLabel;
class QTreeWidget;
class QModelIndex;
class QImage;
class QAction;
class ThumbnailModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void SetImage(const QString& imgPath);
    void OpenFolder(const QString& dirPath);   // switch to browse mode on a folder

    // QWidget interface
protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onFolderSelected(const QModelIndex& index);
    void onThumbSelected(const QModelIndex& index);
    void onThumbActivated(const QModelIndex& index);
    void onOpenFile();
    void onOpenFolder();
    void onAbout();
    void onLicensing();
    void onShortcuts();

private:
    void setupBrowser();
    void setupMenus();
    void showViewMode();
    void showBrowseMode();
    void setFullScreenView(bool on);
    void stepImage(int delta);
    void updateSidePanels(const QString& path);
    QImage loadImage(const QString& path) const;

    Ui::MainWindow*     ui;
    QString             m_currentPath;
    QList<QString>      m_supportedFormats;
    std::shared_ptr<ImageWithVideoBase> m_imageManager;

    // browsing mode
    QStackedWidget*     m_stack;
    QListView*          m_grid;
    ThumbnailModel*     m_thumbs;
    QTreeView*          m_folderTree;
    QFileSystemModel*   m_dirModel;
    QLabel*             m_preview;
    QTreeWidget*        m_metadata;
    QAction*            m_fullScreenAction;
};
#endif // MAINWINDOW_H
