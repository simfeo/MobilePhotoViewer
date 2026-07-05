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
#include <QItemSelectionModel>
#include <QStandardPaths>

#include <QDebug>

#include <jpegmanager.h>
#include <heifmanager.h>
#include <TinyEXIF.h>

#include "thumbnailmodel.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentPath()
    , m_imageManager(nullptr)
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

    // File menu: Open File... / Open Folder...
    ui->actionOpen->setText(tr("Open File..."));
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile);
    QAction* openFolder = new QAction(tr("Open Folder..."), this);
    ui->menuFile->insertAction(ui->actionExit, openFolder);
    connect(openFolder, &QAction::triggered, this, &MainWindow::onOpenFolder);

    showBrowseMode();
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

    QDir d(dir);
    QStringList names = d.entryList(m_supportedFormats, QDir::Files, QDir::Name);
    QStringList paths;
    paths.reserve(names.size());
    for (const QString& n : names)
        paths.append(d.absoluteFilePath(n));

    m_thumbs->setFiles(paths);
    setWindowTitle(dir);
    showBrowseMode();
}

void MainWindow::onThumbSelected(const QModelIndex& index)
{
    updateSidePanels(m_thumbs->filePath(index));
}

void MainWindow::onThumbActivated(const QModelIndex& index)
{
    const QString path = m_thumbs->filePath(index);
    if (!path.isEmpty())
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
// Viewing
// --------------------------------------------------------------------------
void MainWindow::SetImage(const QString& imgPath)
{
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
    if (m_imageManager->HasMicrovideo())
    {
        ui->photoVidget->SetVideoManager(m_imageManager->getVideoMemory());
    }
    setWindowTitle(imgPath);
    showViewMode();
    repaint();
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

    QPoint numDegrees = event->angleDelta() / 8;
    QPoint numSteps = numDegrees / 15;
    if (numSteps.y() != 0 && m_currentPath.size() != 0)
    {
        QDir workingDir = QFileInfo( m_currentPath ).absoluteDir();
        QStringList images = workingDir.entryList(m_supportedFormats,QDir::Files);
        if (images.empty())
        {
            return;
        }
        images.sort();
        int in = images.indexOf(QFileInfo( m_currentPath ).fileName());
        const int current_in = in;
        in = in - numSteps.y();
        if (in < 0)
        {
            in = 0;
        }
        else if (in >= images.size())
        {
            in = images.size()-1;
        }

        if (current_in != in)
        {
            SetImage(workingDir.absoluteFilePath(images[in]));
        }
    }
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
        else if (m_stack->currentWidget() == ui->photoVidget)
            showBrowseMode();   // back to the thumbnails
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}
