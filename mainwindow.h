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
class QAction;
class PanoramaWidget;
class MetadataPanel;
class FolderBrowserPanel;

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
    void onOpenFile();
    void onOpenFolder();
    void onSaveVideo();
    void onAbout();
    void onLicensing();
    void onShortcuts();
    void onSettings();

private:
    void setupBrowser();
    void setupMenus();
    bool detectPanorama(const QString& path);   // fills m_pano* if equirectangular
    void showPanorama();                        // build the sphere and switch to it
    void showViewMode();
    void showBrowseMode();
    void setFullScreenView(bool on);
    void stepImage(int delta);

    Ui::MainWindow*     ui;
    QString             m_currentPath;
    std::shared_ptr<ImageWithVideoBase> m_imageManager;

    // browsing mode
    QStackedWidget*     m_stack;
    FolderBrowserPanel* m_browser;
    MetadataPanel*      m_info;
    QAction*            m_fullScreenAction;
    QAction*            m_saveVideoAction;
    PanoramaWidget*     m_pano;

    // current image's panorama metadata (valid when m_isPano)
    bool m_isPano = false;
    bool m_panoCylindrical = false;   // cylindrical vs equirectangular projection
    int  m_panoFullW = 0, m_panoFullH = 0;
    int  m_panoCropW = 0, m_panoCropH = 0, m_panoCropL = 0, m_panoCropT = 0;
};
#endif // MAINWINDOW_H
