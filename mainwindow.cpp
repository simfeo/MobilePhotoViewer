#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QImageReader>
#include <QBuffer>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QPainter>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QCloseEvent>

#include <QFile>
#include <QFileInfo>
#include <QDir>

#include <QStackedWidget>
#include <QDockWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QKeySequence>
#include <QSignalBlocker>
#include <QApplication>

#include <functional>

#include <jpegmanager.h>
#include <heifmanager.h>
#include <TinyEXIF.h>

#include "panoramawidget.h"
#include "metadatapanel.h"
#include "folderbrowserpanel.h"
#include "settingsdialog.h"
#include "appsettings.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentPath()
    , m_imageManager(nullptr)
    , m_fullScreenAction(nullptr)
    , m_saveVideoAction(nullptr)
    , m_pano(nullptr)
{
    ui->setupUi(this);

    setAcceptDrops(true);

    setupBrowser();
    setupMenus();
    restoreLayout();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --------------------------------------------------------------------------
// Window/layout persistence
//
// Two independent pieces: geometry (size/position) and state (dock + toolbar
// arrangement). Both are saved on close and restored here. Geometry is only ever
// discarded via the --reset-window command-line flag (handled in main(), which
// clears the stored value before we construct); the dock layout can also be reset
// to its as-built default from the View menu (resetWidgetLayout()).
// --------------------------------------------------------------------------
void MainWindow::restoreLayout()
{
    // Snapshot the default arrangement now, before any saved state is applied, so
    // "Reset Layout" has something to restore to.
    m_defaultState = saveState();

    const QByteArray geometry = AppSettings::windowGeometry();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const QByteArray state = AppSettings::windowState();
    if (!state.isEmpty())
        restoreState(state);
}

void MainWindow::resetWidgetLayout()
{
    restoreState(m_defaultState);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    AppSettings::setWindowGeometry(saveGeometry());
    AppSettings::setWindowState(saveState());
    QMainWindow::closeEvent(event);
}

// --------------------------------------------------------------------------
// Browsing mode: folder tree + thumbnail grid + preview + metadata
// --------------------------------------------------------------------------
void MainWindow::setupBrowser()
{
    // Central stack: [0] thumbnail grid (browse), [1] the image/video viewer,
    // [2] the photosphere. The folder browser owns the grid; we just place it.
    m_stack = new QStackedWidget(this);
    m_browser = new FolderBrowserPanel(this);

    m_stack->addWidget(m_browser->gridWidget());   // index 0
    ui->photoVidget->setParent(m_stack);
    m_stack->addWidget(ui->photoVidget);           // index 1
    m_pano = new PanoramaWidget(m_stack);
    m_stack->addWidget(m_pano);                    // index 2 (photospheres)

    // Navigation + sort toolbar sits directly above the content pane (not across
    // the whole window over the docks), like ACDSee's location/sort bar.
    QWidget* central = new QWidget(this);
    QVBoxLayout* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_browser->toolBar());
    centralLayout->addWidget(m_stack);
    setCentralWidget(central);

    // Folders dock (left, top). Object names are required for saveState/restoreState.
    // A dock's toggleViewAction() text tracks its windowTitle, so the View menu
    // entries follow along without being registered separately.
    QDockWidget* foldersDock = new QDockWidget(this);
    m_i18n.add([this, foldersDock]{ foldersDock->setWindowTitle(tr("Folders")); });
    foldersDock->setObjectName("FoldersDock");
    foldersDock->setWidget(m_browser->folderTreeWidget());
    addDockWidget(Qt::LeftDockWidgetArea, foldersDock);

    // Preview + Metadata panels (owned by MetadataPanel, placed in their docks).
    m_info = new MetadataPanel(this);

    // Preview dock (left, below folders)
    QDockWidget* previewDock = new QDockWidget(this);
    m_i18n.add([this, previewDock]{ previewDock->setWindowTitle(tr("Preview")); });
    previewDock->setObjectName("PreviewDock");
    previewDock->setWidget(m_info->previewWidget());
    addDockWidget(Qt::LeftDockWidgetArea, previewDock);
    splitDockWidget(foldersDock, previewDock, Qt::Vertical);

    // Metadata dock (right)
    QDockWidget* metaDock = new QDockWidget(this);
    m_i18n.add([this, metaDock]{ metaDock->setWindowTitle(tr("Metadata")); });
    metaDock->setObjectName("MetadataDock");
    metaDock->setWidget(m_info->metadataWidget());
    addDockWidget(Qt::RightDockWidgetArea, metaDock);

    resizeDocks({ foldersDock }, { 240 }, Qt::Horizontal);

    connect(m_browser, &FolderBrowserPanel::previewRequested, m_info, &MetadataPanel::setFile);
    connect(m_browser, &FolderBrowserPanel::imageActivated, this, &MainWindow::SetImage);
    connect(m_browser, &FolderBrowserPanel::folderChanged, this, [this](const QString& dir) {
        setWindowTitle(dir);
        AppSettings::setLastFolder(dir);   // remember for "start from last folder"
        showBrowseMode();
    });
    connect(ui->photoVidget, &ImageRenderWidget::backRequested, this, &MainWindow::showBrowseMode);
    connect(ui->photoVidget, &ImageRenderWidget::panoramaRequested, this, &MainWindow::showPanorama);

    // Toolbar +/- : zoom whatever the central stack is currently showing.
    connect(m_browser, &FolderBrowserPanel::zoomRequested, this, [this](int steps) {
        if (m_stack->currentWidget() == m_pano)
            steps > 0 ? m_pano->zoomIn() : m_pano->zoomOut();
        else if (m_stack->currentWidget() == ui->photoVidget)
            steps > 0 ? ui->photoVidget->zoomIn() : ui->photoVidget->zoomOut();
        else
            m_browser->zoomThumbnails(steps);
    });

    // Apply saved preferences, then open the configured start folder. A folder or
    // file passed on the command line (handled in main after show()) runs later and
    // overrides this.
    m_browser->setThumbSize(AppSettings::defaultThumbSize());
    m_browser->openFolder(AppSettings::resolvedStartupFolder());

    showBrowseMode();
}

void MainWindow::setupMenus()
{
    // Strings are registered with m_i18n so they can be re-applied live when the
    // language changes (see changeEvent). The .ui-defined menus and actions are
    // included: retranslateUi() is deliberately not used, since it would also
    // reset the window title (which holds the current file path) and the play
    // button's glyph.
    // ---- File ----
    m_i18n.add([this]{ ui->menuFile->setTitle(tr("File")); });
    m_i18n.add([this]{ ui->menuHelp->setTitle(tr("Help")); });
    m_i18n.add([this]{ ui->actionExit->setText(tr("Exit")); });
    m_i18n.add([this]{ ui->actionAbout->setText(tr("About")); });
    m_i18n.add([this]{ ui->actionLicensing->setText(tr("Licensing")); });

    m_i18n.add([this]{ ui->actionOpen->setText(tr("Open File...")); });
    ui->actionOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile);

    QAction* openFolder = new QAction(this);
    m_i18n.add([this, openFolder]{ openFolder->setText(tr("Open Folder...")); });
    openFolder->setShortcut(QKeySequence("Ctrl+Shift+O"));
    ui->menuFile->insertAction(ui->actionExit, openFolder);
    connect(openFolder, &QAction::triggered, this, &MainWindow::onOpenFolder);

    m_saveVideoAction = new QAction(this);
    m_i18n.add([this]{ m_saveVideoAction->setText(tr("Save Video As...")); });
    m_saveVideoAction->setShortcut(QKeySequence("Ctrl+S"));
    m_saveVideoAction->setEnabled(false);   // enabled only for motion photos
    ui->menuFile->insertAction(ui->actionExit, m_saveVideoAction);
    ui->menuFile->insertSeparator(ui->actionExit);
    connect(m_saveVideoAction, &QAction::triggered, this, &MainWindow::onSaveVideo);

    ui->actionExit->setShortcut(QKeySequence("Ctrl+Q"));   // already wired to close() in the .ui

    // ---- Edit (file operations, inserted between File and Help) ----
    QMenu* editMenu = new QMenu(this);
    m_i18n.add([this, editMenu]{ editMenu->setTitle(tr("Edit")); });
    menuBar()->insertMenu(ui->menuHelp->menuAction(), editMenu);
    m_browser->fillEditMenu(editMenu);
    editMenu->addSeparator();
    QAction* settings = editMenu->addAction(QString(), this, &MainWindow::onSettings);
    m_i18n.add([this, settings]{ settings->setText(tr("Settings...")); });
    settings->setShortcut(QKeySequence(QKeySequence::Preferences));   // Ctrl+, on macOS
    settings->setMenuRole(QAction::PreferencesRole);                  // -> app menu on macOS

    // ---- View (inserted between File and Help) ----
    QMenu* viewMenu = new QMenu(this);
    m_i18n.add([this, viewMenu]{ viewMenu->setTitle(tr("View")); });
    menuBar()->insertMenu(ui->menuHelp->menuAction(), viewMenu);

    // Deliberately no setShortcut(): Enter / Esc go back to the thumbnails, but
    // both are handled in keyPressEvent(). As action shortcuts they would be
    // window-wide -- Enter would be stolen from the grid (where it opens an
    // image), and Esc would bypass the "leave full screen first" check.
    QAction* thumbnails = viewMenu->addAction(QString(), this, &MainWindow::showBrowseMode);
    m_i18n.add([this, thumbnails]{ thumbnails->setText(tr("Thumbnails")); });
    viewMenu->addSeparator();

    // Zoom / navigation only make sense in the viewer, so their shortcuts are
    // scoped to it (WidgetWithChildrenShortcut) and don't clash with the grid.
    // The text arrives as a callable so it can be re-evaluated on a language change.
    auto addViewerAction = [this, viewMenu](const std::function<QString()>& text,
                                            const QKeySequence& sc,
                                            const std::function<void()>& fn) {
        QAction* a = new QAction(this);
        m_i18n.add([a, text]{ a->setText(text()); });
        a->setShortcut(sc);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(a, &QAction::triggered, this, fn);
        ui->photoVidget->addAction(a);   // active when either viewer is focused
        m_pano->addAction(a);
        viewMenu->addAction(a);
    };
    const auto inPano = [this]{ return m_stack->currentWidget() == m_pano; };
    addViewerAction([this]{ return tr("Zoom In"); },   QKeySequence(Qt::Key_Plus),  [this, inPano]{ inPano() ? m_pano->zoomIn()  : ui->photoVidget->zoomIn(); });
    addViewerAction([this]{ return tr("Zoom Out"); },  QKeySequence(Qt::Key_Minus), [this, inPano]{ inPano() ? m_pano->zoomOut() : ui->photoVidget->zoomOut(); });
    addViewerAction([this]{ return tr("Fit to Window"); }, QKeySequence("Ctrl+0"),  [this, inPano]{ inPano() ? m_pano->resetView() : ui->photoVidget->resetZoom(); });
    viewMenu->addSeparator();
    addViewerAction([this]{ return tr("Previous Image"); }, QKeySequence(Qt::Key_Left),  [this]{ stepImage(-1); });
    addViewerAction([this]{ return tr("Next Image"); },     QKeySequence(Qt::Key_Right), [this]{ stepImage(1); });
    addViewerAction([this]{ return tr("Play Video"); },     QKeySequence(Qt::Key_Space), [this]{ ui->photoVidget->playVideo(); });
    addViewerAction([this]{ return tr("Hide/Show Play Button"); }, QKeySequence(Qt::Key_H), [this]{ ui->photoVidget->togglePlayButton(); });
    viewMenu->addSeparator();

    m_fullScreenAction = new QAction(this);
    m_i18n.add([this]{ m_fullScreenAction->setText(tr("Full Screen")); });
    m_fullScreenAction->setCheckable(true);
    m_fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));   // 'F' is handled in keyPressEvent too
    connect(m_fullScreenAction, &QAction::triggered, this, &MainWindow::setFullScreenView);
    viewMenu->addAction(m_fullScreenAction);
    viewMenu->addSeparator();

    viewMenu->addAction(m_browser->toolBar()->toggleViewAction());   // Navigation toolbar
    for (QDockWidget* d : findChildren<QDockWidget*>())
        viewMenu->addAction(d->toggleViewAction());   // Folders / Preview / Metadata

    viewMenu->addSeparator();
    QAction* resetLayout = viewMenu->addAction(QString(), this, &MainWindow::resetWidgetLayout);
    m_i18n.add([this, resetLayout]{ resetLayout->setText(tr("Reset Layout")); });

    // ---- Help ----
    QAction* shortcuts = new QAction(this);
    m_i18n.add([this, shortcuts]{ shortcuts->setText(tr("Keyboard Shortcuts")); });
    shortcuts->setShortcut(QKeySequence("F1"));
    connect(shortcuts, &QAction::triggered, this, &MainWindow::onShortcuts);
    ui->menuHelp->insertAction(ui->actionAbout, shortcuts);
    ui->menuHelp->insertSeparator(ui->actionAbout);

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui->actionLicensing, &QAction::triggered, this, &MainWindow::onLicensing);
    QAction* aboutQt = ui->menuHelp->addAction(QString());
    m_i18n.add([this, aboutQt]{ aboutQt->setText(tr("About Qt")); });
    connect(aboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::showViewMode()
{
    m_stack->setCurrentWidget(ui->photoVidget);
    ui->photoVidget->setFocus();
}

void MainWindow::showBrowseMode()
{
    m_stack->setCurrentWidget(m_browser->gridWidget());
    m_browser->gridWidget()->setFocus();
    // Land on the image that was open (arrow keys may have moved it on from the
    // one activated), scrolled into view.
    m_browser->selectFile(m_currentPath);
}

void MainWindow::OpenFolder(const QString& dirPath)
{
    m_browser->openFolder(dirPath);   // public entry point (command line, drag & drop)
}

void MainWindow::onOpenFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open image"), QString(),
        tr("Images (*.jpg *.jpeg *.png *.heic *.heif *.bmp *.gif *.webp);;All files (*.*)"));
    if (!path.isEmpty())
        SetImage(path);
}

void MainWindow::onOpenFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open folder"));
    if (!dir.isEmpty())
        OpenFolder(dir);
}

// --------------------------------------------------------------------------
// Panorama / photosphere
// --------------------------------------------------------------------------
// Detect an equirectangular panorama and remember its geometry (used later by
// showPanorama() when the user clicks the 360 button).
bool MainWindow::detectPanorama(const QString& path)
{
    m_isPano = false;
    const QString lower = path.toLower();
    if (!(lower.endsWith("jpg") || lower.endsWith("jpeg")))
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(512 * 1024);   // EXIF/XMP sit near the start of the file
    f.close();

    TinyEXIF::EXIFInfo exif(reinterpret_cast<const uint8_t*>(head.constData()), head.size());
    const bool equirect = exif.GPano.isEquirectangular();
    const bool gpanoCyl =
        QString::fromStdString(exif.GPano.ProjectionType).compare("cylindrical", Qt::CaseInsensitive) == 0;
    // Google/Pixel sweep panoramas carry no GPano geometry -- just a GCamera
    // special-type marker. Treat those as cylindrical with heuristic geometry.
    const bool specialPano = head.contains("SpecialType-PANORAMA");
    if (!equirect && !gpanoCyl && !specialPano)
        return false;

    m_isPano     = true;
    m_panoCylindrical = !equirect && (gpanoCyl || specialPano);
    m_panoFullW  = static_cast<int>(exif.GPano.FullPanoWidthPixels);
    m_panoFullH  = static_cast<int>(exif.GPano.FullPanoHeightPixels);
    m_panoCropW  = static_cast<int>(exif.GPano.CroppedAreaImageWidthPixels);
    m_panoCropH  = static_cast<int>(exif.GPano.CroppedAreaImageHeightPixels);
    m_panoCropL  = static_cast<int>(exif.GPano.CroppedAreaLeftPixels);
    m_panoCropT  = static_cast<int>(exif.GPano.CroppedAreaTopPixels);
    return true;
}

void MainWindow::showPanorama()
{
    if (!m_isPano)
        return;

    QImageReader r(m_currentPath);
    r.setAutoTransform(true);
    QImage img = r.read();
    if (img.isNull())
        return;

    if (m_panoCylindrical)
    {
        // Cylindrical panorama: texture the crop straight onto a cylinder arc.
        const int cropW = m_panoCropW > 0 ? m_panoCropW : img.width();
        const int cropH = m_panoCropH > 0 ? m_panoCropH : img.height();

        const int maxW = 8192;
        const double scale = cropW > maxW ? double(maxW) / cropW : 1.0;
        QImage piece = scale < 1.0
            ? img.scaled(qMax(1, int(cropW * scale)), qMax(1, int(cropH * scale)),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            : img;

        const float PI = 3.14159265358979f;
        float fovRad, halfH;
        if (m_panoFullW > 0)
        {
            // GPano geometry: FullPanoWidthPixels spans 360 deg, so the crop covers
            // width/full of it; square pixels then fix the vertical extent.
            fovRad = 2.0f * PI * float(cropW) / float(m_panoFullW);
            halfH  = PI * float(cropH) / float(m_panoFullW);
        }
        else
        {
            // No projection metadata (e.g. Pixel SpecialType panorama): assume a
            // cylindrical sweep with ~50 deg vertical FoV and derive the horizontal
            // FoV from the image aspect (square pixels), capped at a full wrap.
            halfH  = 0.466f;   // tan(25 deg)
            fovRad = 2.0f * halfH * float(cropW) / float(cropH);
            if (fovRad > 2.0f * PI)
            {
                fovRad = 2.0f * PI;
                halfH  = fovRad * float(cropH) / (2.0f * float(cropW));
            }
        }

        m_pano->setPanorama(piece.convertToFormat(QImage::Format_RGBA8888),
                            PanoramaWidget::Projection::Cylindrical, halfH, fovRad);
        m_stack->setCurrentWidget(m_pano);
        m_pano->setFocus();
        return;
    }

    int fullW = m_panoFullW, fullH = m_panoFullH;
    int cropW = m_panoCropW, cropH = m_panoCropH, cropL = m_panoCropL, cropT = m_panoCropT;
    if (fullW <= 0 || fullH <= 0) { fullW = img.width(); fullH = img.height(); cropW = fullW; cropH = fullH; cropL = cropT = 0; }
    if (cropW <= 0) cropW = img.width();
    if (cropH <= 0) cropH = img.height();

    // Cap the texture to a safe GL size, keeping the full 2:1 canvas ratio.
    const int maxW = 8192;
    const double scale = fullW > maxW ? double(maxW) / fullW : 1.0;
    const int cw = qMax(1, int(fullW * scale));
    const int ch = qMax(1, int(fullH * scale));

    QImage canvas;
    if (cropW == fullW && cropH == fullH && cropL == 0 && cropT == 0)
    {
        canvas = (scale < 1.0) ? img.scaled(cw, ch, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) : img;
    }
    else
    {
        // Partial panorama: place the captured area on the full black sphere canvas.
        canvas = QImage(cw, ch, QImage::Format_RGB888);
        canvas.fill(Qt::black);
        QImage piece = img.scaled(qMax(1, int(cropW * scale)), qMax(1, int(cropH * scale)),
                                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QPainter p(&canvas);
        p.drawImage(int(cropL * scale), int(cropT * scale), piece);
    }

    m_pano->setPanorama(canvas.convertToFormat(QImage::Format_RGBA8888));
    m_stack->setCurrentWidget(m_pano);
    m_pano->setFocus();
}

// --------------------------------------------------------------------------
// Viewing
// --------------------------------------------------------------------------
void MainWindow::SetImage(const QString& imgPath)
{
    m_currentPath = imgPath;
    if (m_saveVideoAction)
        m_saveVideoAction->setEnabled(false);

    ui->photoVidget->SetPlayButtonVisible(false);
    ui->photoVidget->SetPanoramaButtonVisible(false);
    QImageReader imgReader(imgPath);
    imgReader.setAutoTransform(true);
    QImage qimg = imgReader.read();
    m_currentPath = imgPath;

    ui->photoVidget->SetPixmap(QPixmap::fromImage(qimg));
    const QString lower = imgPath.toLower();   // extensions may be upper-case (.HEIC, .JPG)
    if (lower.endsWith("jpeg") || lower.endsWith("jpg"))
    {
        m_imageManager.reset(new JpegManager(imgPath));
    }
    else if (lower.endsWith("heic") || lower.endsWith("heif"))
    {
        m_imageManager.reset(new HeifManager(imgPath));

        if (m_imageManager->getWidth()  && m_imageManager->getHeight())
        {
            QImage qimg = QImage(m_imageManager->getDecodedImage().get(), static_cast<int>(m_imageManager->getWidth()), static_cast<int>(m_imageManager->getHeight()), QImage::Format_RGBA8888);
            ui->photoVidget->SetPixmap(QPixmap::fromImage(qimg));
        }
    }
    else
    {
        m_imageManager.reset(new ImageWithVideoBase());
    }
    const bool hasVideo = m_imageManager->HasMicrovideo();
    if (hasVideo)
        ui->photoVidget->SetVideoManager(m_imageManager->getVideoMemory());
    if (m_saveVideoAction)
        m_saveVideoAction->setEnabled(hasVideo);

    // Equirectangular panoramas open flat, with a 360 button to enter the sphere.
    ui->photoVidget->SetPanoramaButtonVisible(detectPanorama(imgPath));

    // Keep the side panels in sync with whatever image is on screen -- this also
    // covers paths that bypass grid selection (drag & drop, Open File, stepImage).
    m_info->setFile(imgPath);

    setWindowTitle(imgPath);
    showViewMode();
    repaint();
}

void MainWindow::onSaveVideo()
{
    if (!m_imageManager || !m_imageManager->HasMicrovideo())
        return;
    std::shared_ptr<QBuffer> vid = m_imageManager->getVideoMemory();
    if (!vid)
        return;

    const QString suggested = QFileInfo(m_currentPath).completeBaseName() + ".mp4";
    const QString out = QFileDialog::getSaveFileName(
        this, tr("Save video"),
        QFileInfo(m_currentPath).absoluteDir().absoluteFilePath(suggested),
        tr("Video (*.mp4 *.3gp);;All files (*.*)"));
    if (out.isEmpty())
        return;

    QFile f(out);
    if (f.open(QIODevice::WriteOnly))
        f.write(vid->data());
    else
        QMessageBox::warning(this, tr("Save video"), tr("Could not write %1").arg(out));
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        const QString path = mimeData->urls().front().toLocalFile();
        if (QFileInfo(path).isDir())
        {
            OpenFolder(path);
        }
        else
        {
            OpenFolder(QFileInfo(path).absolutePath());   // browse folder follows the photo
            SetImage(path);
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    if (m_stack->currentWidget() != ui->photoVidget)
        return;   // wheel only steps through images in view mode

    const QPoint numSteps = event->angleDelta() / (8 * 15);
    if (numSteps.y() != 0)
        stepImage(-numSteps.y());   // wheel up -> previous
}

void MainWindow::setFullScreenView(bool on)
{
    // Immersive full screen: drop the menu bar, the toolbar and the browse docks.
    menuBar()->setVisible(!on);
    m_browser->toolBar()->setVisible(!on);
    for (QDockWidget* d : findChildren<QDockWidget*>())
        d->setVisible(!on);
    if (on)
        showFullScreen();
    else
        showNormal();
    if (m_fullScreenAction && m_fullScreenAction->isChecked() != on)
    {
        QSignalBlocker block(m_fullScreenAction);
        m_fullScreenAction->setChecked(on);
    }
}

void MainWindow::stepImage(int delta)
{
    if (m_currentPath.isEmpty())
        return;
    QDir dir = QFileInfo(m_currentPath).absoluteDir();
    QStringList images = dir.entryList(m_browser->supportedFormats(), QDir::Files, QDir::Name);
    if (images.isEmpty())
        return;
    const int idx = images.indexOf(QFileInfo(m_currentPath).fileName());
    const int next = qBound(0, idx + delta, images.size() - 1);
    if (next != idx)
        SetImage(dir.absoluteFilePath(images[next]));
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        // Apply the (possibly new) default thumbnail size to the live grid.
        m_browser->setThumbSize(AppSettings::defaultThumbSize());
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About MobilePhotoViewer"),
        tr("<h3>MobilePhotoViewer</h3>"
           "<p>Viewer for phone photos and <b>motion photos</b> &mdash; Samsung / Google "
           "JPEG and HEIF, with playback of the embedded video clip.</p>"));
}

void MainWindow::onLicensing()
{
    QMessageBox::about(this, tr("Licensing"),
        tr("<p>Built with these third-party components:</p>"
           "<ul>"
           "<li>Qt 6 &mdash; LGPLv3</li>"
           "<li>libheif &mdash; LGPLv3</li>"
           "<li>libde265 &mdash; LGPLv3</li>"
           "<li>tinyxml2 &mdash; zlib</li>"
           "<li>TinyEXIF &mdash; BSD</li>"
           "</ul>"));
}

void MainWindow::onShortcuts()
{
    QMessageBox::information(this, tr("Keyboard Shortcuts"),
        tr("<table cellpadding='4'>"
           "<tr><td><b>Ctrl+O</b></td><td>Open file</td></tr>"
           "<tr><td><b>Ctrl+Shift+O</b></td><td>Open folder</td></tr>"
           "<tr><td><b>Ctrl+S</b></td><td>Save video (motion photos)</td></tr>"
           "<tr><td><b>Enter</b> / double-click</td><td>Open the selected thumbnail</td></tr>"
           "<tr><td><b>Enter</b> / <b>Esc</b></td><td>Back to thumbnails</td></tr>"
           "<tr><td><b>+</b> / <b>&minus;</b></td><td>Zoom in / out</td></tr>"
           "<tr><td><b>Ctrl+0</b></td><td>Fit to window</td></tr>"
           "<tr><td>drag</td><td>Pan while zoomed</td></tr>"
           "<tr><td><b>&larr;</b> / <b>&rarr;</b> / wheel</td><td>Previous / next image</td></tr>"
           "<tr><td><b>Space</b></td><td>Play video</td></tr>"
           "<tr><td><b>H</b></td><td>Hide / show play button</td></tr>"
           "<tr><td><b>F</b> / <b>F11</b></td><td>Full screen</td></tr>"
           "<tr><td><b>Ctrl+Q</b></td><td>Quit</td></tr>"
           "</table>"));
}

void MainWindow::changeEvent(QEvent *event)
{
    // Qt sends this to every top-level widget when the translators change, and
    // QWidget forwards it to children, so each panel retranslates itself.
    if (event->type() == QEvent::LanguageChange)
        m_i18n.retranslate();
    QMainWindow::changeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F)
    {
        setFullScreenView(!isFullScreen());
    }
    else if (event->key() == Qt::Key_Escape)
    {
        if (isFullScreen())
            setFullScreenView(false);
        else if (m_stack->currentWidget() != m_browser->gridWidget())
            showBrowseMode();   // back to the thumbnails (from image or pano view)
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        // Enter mirrors the viewer's double-click: back to the thumbnails. The
        // grid consumes Enter itself (to open the current thumbnail), so this is
        // only ever reached from the image / panorama view.
        if (m_stack->currentWidget() != m_browser->gridWidget())
            showBrowseMode();
        else
            QMainWindow::keyPressEvent(event);
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}
