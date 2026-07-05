#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLabel>
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
#include <QFileInfoList>
#include <QDir>

#include <QStackedWidget>
#include <QListView>
#include <QTreeView>
#include <QFileSystemModel>
#include <QTreeWidget>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QKeySequence>
#include <QSignalBlocker>
#include <QApplication>

#include <functional>
#include <QItemSelectionModel>
#include <QStandardPaths>

#include <QDebug>

#include <jpegmanager.h>
#include <heifmanager.h>
#include <TinyEXIF.h>

#include "thumbnailmodel.h"
#include "panoramawidget.h"

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
    for (auto& i : QImageReader::supportedImageFormats())
    {
        m_supportedFormats.append("*."+i);
    }
    m_supportedFormats.append("*.heic");
    m_supportedFormats.append("*.heif");

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
    // Central stack: [0] thumbnail grid (browse), [1] the image/video viewer.
    m_stack = new QStackedWidget(this);

    m_thumbs = new ThumbnailModel(this);
    m_grid = new QListView(m_stack);
    m_grid->setModel(m_thumbs);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setIconSize(QSize(128, 128));
    m_grid->setGridSize(QSize(150, 150));
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setUniformItemSizes(true);
    m_grid->setMovement(QListView::Static);
    m_grid->setSpacing(6);
    m_grid->setWordWrap(true);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);

    m_stack->addWidget(m_grid);                 // index 0
    ui->photoVidget->setParent(m_stack);
    m_stack->addWidget(ui->photoVidget);        // index 1
    m_pano = new PanoramaWidget(m_stack);
    m_stack->addWidget(m_pano);                 // index 2 (photospheres)
    setCentralWidget(m_stack);

    // Folders dock (left, top)
    m_dirModel = new QFileSystemModel(this);
    m_dirModel->setFilter(QDir::Dirs | QDir::Drives | QDir::NoDotAndDotDot);
    m_dirModel->setRootPath("");
    m_folderTree = new QTreeView;
    m_folderTree->setModel(m_dirModel);
    m_folderTree->setHeaderHidden(true);
    for (int c = 1; c < m_dirModel->columnCount(); ++c)
        m_folderTree->setColumnHidden(c, true);
    QDockWidget* foldersDock = new QDockWidget(tr("Folders"), this);
    foldersDock->setWidget(m_folderTree);
    addDockWidget(Qt::LeftDockWidgetArea, foldersDock);

    // Preview dock (left, below folders)
    m_preview = new QLabel;
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(180);
    m_preview->setStyleSheet("background:#202020;");
    QDockWidget* previewDock = new QDockWidget(tr("Preview"), this);
    previewDock->setWidget(m_preview);
    addDockWidget(Qt::LeftDockWidgetArea, previewDock);
    splitDockWidget(foldersDock, previewDock, Qt::Vertical);

    // Metadata dock (right)
    m_metadata = new QTreeWidget;
    m_metadata->setColumnCount(2);
    m_metadata->setHeaderLabels({ tr("Property"), tr("Value") });
    m_metadata->setRootIsDecorated(false);
    m_metadata->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    QDockWidget* metaDock = new QDockWidget(tr("Metadata"), this);
    metaDock->setWidget(m_metadata);
    addDockWidget(Qt::RightDockWidgetArea, metaDock);

    resizeDocks({ foldersDock }, { 240 }, Qt::Horizontal);

    connect(m_folderTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onFolderSelected);
    connect(m_grid->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onThumbSelected);
    connect(m_grid, &QListView::doubleClicked, this, &MainWindow::onThumbActivated);
    connect(ui->photoVidget, &ImageRenderWidget::backRequested, this, &MainWindow::showBrowseMode);

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
    m_stack->setCurrentWidget(m_grid);
    m_grid->setFocus();
}

void MainWindow::OpenFolder(const QString& dirPath)
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

void MainWindow::onFolderSelected(const QModelIndex& index)
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
    setWindowTitle(dir);
    showBrowseMode();
}

void MainWindow::onThumbSelected(const QModelIndex& index)
{
    if (!m_thumbs->isDir(index))
        updateSidePanels(m_thumbs->filePath(index));
    else
        updateSidePanels(QString());
}

void MainWindow::onThumbActivated(const QModelIndex& index)
{
    const QString path = m_thumbs->filePath(index);
    if (path.isEmpty())
        return;
    if (m_thumbs->isDir(index))
        OpenFolder(path);   // navigate into the sub-folder
    else
        SetImage(path);
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

QImage MainWindow::loadImage(const QString& path) const
{
    const QString lower = path.toLower();
    if (lower.endsWith("heic") || lower.endsWith("heif"))
    {
        HeifManager m(path);
        if (m.getWidth() && m.getHeight())
        {
            QImage img(m.getDecodedImage().get(),
                       static_cast<int>(m.getWidth()), static_cast<int>(m.getHeight()),
                       QImage::Format_RGBA8888);
            return img.copy();   // own the pixels; HeifManager frees its buffer
        }
        return QImage();
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read();
}

void MainWindow::updateSidePanels(const QString& path)
{
    m_metadata->clear();
    if (path.isEmpty())
    {
        m_preview->clear();
        return;
    }

    // Preview (scaled).
    QImage img = loadImage(path);
    if (!img.isNull())
    {
        const QSize box = m_preview->size().isValid() && m_preview->width() > 20
                              ? m_preview->size() : QSize(240, 240);
        m_preview->setPixmap(QPixmap::fromImage(
            img.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
    else
    {
        m_preview->setText(tr("No preview"));
    }

    // Metadata.
    auto add = [this](const QString& k, const QString& v) {
        if (!v.isEmpty())
            new QTreeWidgetItem(m_metadata, { k, v });
    };
    QFileInfo fi(path);
    add(tr("Name"), fi.fileName());
    add(tr("Size"), QString::number(fi.size() / 1024) + " KB");
    add(tr("Modified"), fi.lastModified().toString(Qt::ISODate));
    if (!img.isNull())
        add(tr("Dimensions"), QString("%1 x %2").arg(img.width()).arg(img.height()));

    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
    {
        const QByteArray bytes = f.read(256 * 1024);   // header is enough for EXIF
        TinyEXIF::EXIFInfo exif(reinterpret_cast<const uint8_t*>(bytes.constData()), bytes.size());
        if (exif.Fields)
        {
            const QString make = QString::fromStdString(exif.Make);
            const QString model = QString::fromStdString(exif.Model);
            add(tr("Camera"), (make + " " + model).trimmed());
            add(tr("Date"), QString::fromStdString(exif.DateTimeOriginal));
            if (exif.ExposureTime > 0)
                add(tr("Exposure"), QString("1/%1 s").arg(qRound(1.0 / exif.ExposureTime)));
            if (exif.FNumber > 0)
                add(tr("F-number"), "f/" + QString::number(exif.FNumber, 'g', 3));
            if (exif.ISOSpeedRatings > 0)
                add(tr("ISO"), QString::number(exif.ISOSpeedRatings));
            if (exif.FocalLength > 0)
                add(tr("Focal length"), QString::number(exif.FocalLength, 'g', 3) + " mm");
            if (exif.GeoLocation.hasLatLon())
                add(tr("GPS"), QString("%1, %2")
                        .arg(exif.GeoLocation.Latitude).arg(exif.GeoLocation.Longitude));
        }
    }
}

// --------------------------------------------------------------------------
// Panorama / photosphere
// --------------------------------------------------------------------------
bool MainWindow::tryOpenPanorama(const QString& path)
{
    const QString lower = path.toLower();
    if (!(lower.endsWith("jpg") || lower.endsWith("jpeg")))
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(512 * 1024);   // EXIF/XMP sit near the start of the file
    f.close();

    TinyEXIF::EXIFInfo exif(reinterpret_cast<const uint8_t*>(head.constData()), head.size());
    if (!exif.GPano.isEquirectangular())
        return false;   // not an equirectangular pano -> show it flat

    int fullW = static_cast<int>(exif.GPano.FullPanoWidthPixels);
    int fullH = static_cast<int>(exif.GPano.FullPanoHeightPixels);
    int cropW = static_cast<int>(exif.GPano.CroppedAreaImageWidthPixels);
    int cropH = static_cast<int>(exif.GPano.CroppedAreaImageHeightPixels);
    int cropL = static_cast<int>(exif.GPano.CroppedAreaLeftPixels);
    int cropT = static_cast<int>(exif.GPano.CroppedAreaTopPixels);

    QImageReader r(path);
    r.setAutoTransform(true);
    QImage img = r.read();
    if (img.isNull())
        return false;

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
    return true;
}

// --------------------------------------------------------------------------
// Viewing
// --------------------------------------------------------------------------
void MainWindow::SetImage(const QString& imgPath)
{
    m_currentPath = imgPath;
    if (m_saveVideoAction)
        m_saveVideoAction->setEnabled(false);

    // Equirectangular photospheres/panoramas are shown in the spherical viewer.
    if (tryOpenPanorama(imgPath))
    {
        setWindowTitle(imgPath);
        return;
    }

    ui->photoVidget->SetPlayButtonVisible(false);
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
            OpenFolder(path);
        else
            SetImage(path);
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
    // Immersive full screen: drop the menu bar and the browse docks.
    menuBar()->setVisible(!on);
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
    QStringList images = dir.entryList(m_supportedFormats, QDir::Files, QDir::Name);
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
        else if (m_stack->currentWidget() != m_grid)
            showBrowseMode();   // back to the thumbnails (from image or pano view)
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}
