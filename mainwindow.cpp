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
}

MainWindow::~MainWindow()
{
    delete ui;
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

    // Folders dock (left, top)
    QDockWidget* foldersDock = new QDockWidget(tr("Folders"), this);
    foldersDock->setWidget(m_browser->folderTreeWidget());
    addDockWidget(Qt::LeftDockWidgetArea, foldersDock);

    // Preview + Metadata panels (owned by MetadataPanel, placed in their docks).
    m_info = new MetadataPanel(this);

    // Preview dock (left, below folders)
    QDockWidget* previewDock = new QDockWidget(tr("Preview"), this);
    previewDock->setWidget(m_info->previewWidget());
    addDockWidget(Qt::LeftDockWidgetArea, previewDock);
    splitDockWidget(foldersDock, previewDock, Qt::Vertical);

    // Metadata dock (right)
    QDockWidget* metaDock = new QDockWidget(tr("Metadata"), this);
    metaDock->setWidget(m_info->metadataWidget());
    addDockWidget(Qt::RightDockWidgetArea, metaDock);

    resizeDocks({ foldersDock }, { 240 }, Qt::Horizontal);

    connect(m_browser, &FolderBrowserPanel::previewRequested, m_info, &MetadataPanel::setFile);
    connect(m_browser, &FolderBrowserPanel::imageActivated, this, &MainWindow::SetImage);
    connect(m_browser, &FolderBrowserPanel::folderChanged, this, [this](const QString& dir) {
        setWindowTitle(dir);
        showBrowseMode();
    });
    connect(ui->photoVidget, &ImageRenderWidget::backRequested, this, &MainWindow::showBrowseMode);
    connect(ui->photoVidget, &ImageRenderWidget::panoramaRequested, this, &MainWindow::showPanorama);

    showBrowseMode();
}

void MainWindow::setupMenus()
{
    // ---- File ----
    ui->actionOpen->setText(tr("Open File..."));
    ui->actionOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile);

    QAction* openFolder = new QAction(tr("Open Folder..."), this);
    openFolder->setShortcut(QKeySequence("Ctrl+Shift+O"));
    ui->menuFile->insertAction(ui->actionExit, openFolder);
    connect(openFolder, &QAction::triggered, this, &MainWindow::onOpenFolder);

    m_saveVideoAction = new QAction(tr("Save Video As..."), this);
    m_saveVideoAction->setShortcut(QKeySequence("Ctrl+S"));
    m_saveVideoAction->setEnabled(false);   // enabled only for motion photos
    ui->menuFile->insertAction(ui->actionExit, m_saveVideoAction);
    ui->menuFile->insertSeparator(ui->actionExit);
    connect(m_saveVideoAction, &QAction::triggered, this, &MainWindow::onSaveVideo);

    ui->actionExit->setShortcut(QKeySequence("Ctrl+Q"));   // already wired to close() in the .ui

    // ---- Edit (file operations, inserted between File and Help) ----
    QMenu* editMenu = new QMenu(tr("Edit"), this);
    menuBar()->insertMenu(ui->menuHelp->menuAction(), editMenu);
    m_browser->fillEditMenu(editMenu);

    // ---- View (inserted between File and Help) ----
    QMenu* viewMenu = new QMenu(tr("View"), this);
    menuBar()->insertMenu(ui->menuHelp->menuAction(), viewMenu);

    QAction* browse = viewMenu->addAction(tr("Thumbnails"), this, &MainWindow::showBrowseMode);
    browse->setShortcut(QKeySequence("Ctrl+B"));
    viewMenu->addSeparator();

    // Zoom / navigation only make sense in the viewer, so their shortcuts are
    // scoped to it (WidgetWithChildrenShortcut) and don't clash with the grid.
    auto addViewerAction = [this, viewMenu](const QString& text, const QKeySequence& sc,
                                            const std::function<void()>& fn) {
        QAction* a = new QAction(text, this);
        a->setShortcut(sc);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(a, &QAction::triggered, this, fn);
        ui->photoVidget->addAction(a);   // active when either viewer is focused
        m_pano->addAction(a);
        viewMenu->addAction(a);
    };
    const auto inPano = [this]{ return m_stack->currentWidget() == m_pano; };
    addViewerAction(tr("Zoom In"),   QKeySequence(Qt::Key_Plus),  [this, inPano]{ inPano() ? m_pano->zoomIn()  : ui->photoVidget->zoomIn(); });
    addViewerAction(tr("Zoom Out"),  QKeySequence(Qt::Key_Minus), [this, inPano]{ inPano() ? m_pano->zoomOut() : ui->photoVidget->zoomOut(); });
    addViewerAction(tr("Fit to Window"), QKeySequence("Ctrl+0"),  [this, inPano]{ inPano() ? m_pano->resetView() : ui->photoVidget->resetZoom(); });
    viewMenu->addSeparator();
    addViewerAction(tr("Previous Image"), QKeySequence(Qt::Key_Left),  [this]{ stepImage(-1); });
    addViewerAction(tr("Next Image"),     QKeySequence(Qt::Key_Right), [this]{ stepImage(1); });
    addViewerAction(tr("Play Video"),     QKeySequence(Qt::Key_Space), [this]{ ui->photoVidget->playVideo(); });
    addViewerAction(tr("Hide/Show Play Button"), QKeySequence(Qt::Key_H), [this]{ ui->photoVidget->togglePlayButton(); });
    viewMenu->addSeparator();

    m_fullScreenAction = new QAction(tr("Full Screen"), this);
    m_fullScreenAction->setCheckable(true);
    m_fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));   // 'F' is handled in keyPressEvent too
    connect(m_fullScreenAction, &QAction::triggered, this, &MainWindow::setFullScreenView);
    viewMenu->addAction(m_fullScreenAction);
    viewMenu->addSeparator();

    viewMenu->addAction(m_browser->toolBar()->toggleViewAction());   // Navigation toolbar
    for (QDockWidget* d : findChildren<QDockWidget*>())
        viewMenu->addAction(d->toggleViewAction());   // Folders / Preview / Metadata

    // ---- Help ----
    QAction* shortcuts = new QAction(tr("Keyboard Shortcuts"), this);
    shortcuts->setShortcut(QKeySequence("F1"));
    connect(shortcuts, &QAction::triggered, this, &MainWindow::onShortcuts);
    ui->menuHelp->insertAction(ui->actionAbout, shortcuts);
    ui->menuHelp->insertSeparator(ui->actionAbout);

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui->actionLicensing, &QAction::triggered, this, &MainWindow::onLicensing);
    QAction* aboutQt = ui->menuHelp->addAction(tr("About Qt"));
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
    const bool cylindrical =
        QString::fromStdString(exif.GPano.ProjectionType).compare("cylindrical", Qt::CaseInsensitive) == 0;
    if (!exif.GPano.isEquirectangular() && !cylindrical)
        return false;

    m_isPano     = true;
    m_panoCylindrical = cylindrical;
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
        // FullPanoWidthPixels spans 360 deg, so the crop covers width/full of it;
        // square-pixel geometry then fixes the vertical extent (no FullPanoHeight).
        const int fullW = m_panoFullW > 0 ? m_panoFullW : img.width();
        const int cropW = m_panoCropW > 0 ? m_panoCropW : img.width();
        const int cropH = m_panoCropH > 0 ? m_panoCropH : img.height();

        const int maxW = 8192;
        const double scale = cropW > maxW ? double(maxW) / cropW : 1.0;
        QImage piece = scale < 1.0
            ? img.scaled(qMax(1, int(cropW * scale)), qMax(1, int(cropH * scale)),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            : img;

        const float PI = 3.14159265358979f;
        const float fovRad = 2.0f * PI * float(cropW) / float(fullW);
        const float halfH  = PI * float(cropH) / float(fullW);

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
    if (imgPath.endsWith("jpeg") || imgPath.endsWith("jpg"))
    {
        m_imageManager.reset(new JpegManager(imgPath));
    }
    else if (imgPath.endsWith("heic") || imgPath.endsWith("heif"))
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
           "<tr><td><b>Ctrl+B</b> / <b>Esc</b></td><td>Back to thumbnails</td></tr>"
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
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}
