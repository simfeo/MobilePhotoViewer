#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QImage>
#include <QMainWindow>
#include <QString>
#include <QByteArray>
#include "formats/imagewithvideobase.h"
#include "app/i18n.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
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
class DepthWidget;
class MetadataPanel;
class FolderBrowserPanel;
class HeifManager;

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
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;   // persist window layout
    void changeEvent(QEvent* event) override;       // QEvent::LanguageChange -> retranslate

private slots:
    void onOpenFile();
    void onOpenFolder();
    void onSaveVideo();
    void onAbout();
    void onLicensing();
    void onShortcuts();
    void onSettings();
    void resetWidgetLayout();   // restore docks/toolbar to their default arrangement

private:
    void setupBrowser();
    void setupMenus();
    void restoreLayout();                       // capture default state, then apply any saved layout
    bool detectPanorama(const QString& path, const QByteArray& fileData);   // fills m_pano* if equirectangular
    void showPanorama();                        // build the sphere and switch to it
    void showDepth();                           // feed the parallax viewer and switch to it
    void showViewMode();
    void applyImageManager();   // video, depth and source-frame controls for m_imageManager
    void loadHeifAsync(const QByteArray& fileData);
    void onHeifLoaded(const std::shared_ptr<HeifManager>& manager, const QImage& img, int generation);
    void switchSourceFrame();   // cycle the still through the SEF source frames
    void showBrowseMode();
    void setFullScreenView(bool on);
    void stepImage(int delta);

    Ui::MainWindow* ui;
    QString         m_currentPath;
    QByteArray      m_defaultState;   // dock/toolbar layout as first built
    Retranslator    m_i18n;           // re-applies menu/dock texts on language change
    // Window state (maximized or not) as it was before going full screen, so
    // leaving full screen restores it instead of always dropping to normal.
    Qt::WindowStates                    m_preFullScreenState = Qt::WindowNoState;
    std::shared_ptr<ImageWithVideoBase> m_imageManager;
    // Source-frame switcher: 0 is the still as shipped, 1..n the SEF frames.
    int    m_frameIndex = 0;
    QImage m_baseFrame;
    int    m_loadGeneration = 0;   // bumped per SetImage so a late background decode is dropped

    // browsing mode
    QStackedWidget*     m_stack;
    FolderBrowserPanel* m_browser;
    MetadataPanel*      m_info;
    QAction*            m_fullScreenAction;
    QAction*            m_saveVideoAction;
    PanoramaWidget*     m_pano;
    DepthWidget*        m_depth;

    // current image's panorama metadata (valid when m_isPano)
    bool m_isPano          = false;
    bool m_panoCylindrical = false;   // cylindrical vs equirectangular projection
    int  m_panoFullW = 0, m_panoFullH = 0;
    int  m_panoCropW = 0, m_panoCropH = 0, m_panoCropL = 0, m_panoCropT = 0;
};
#endif   // MAINWINDOW_H
