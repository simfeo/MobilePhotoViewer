#include "metadatapanel.h"

#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>
#include <QImage>
#include <QImageReader>
#include "formats/jpegrepair.h"
#include <QPixmap>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

#include "formats/heifpreview.h"
#include <TinyEXIF.h>

MetadataPanel::MetadataPanel(QObject* parent)
    : QObject(parent), m_preview(new QLabel), m_metadata(new QTreeWidget)
{
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(180);
    m_preview->setStyleSheet("background:#202020;");

    m_metadata->setColumnCount(2);
    m_metadata->setHeaderLabels({ tr("Property"), tr("Value") });
    m_metadata->setRootIsDecorated(false);
    m_metadata->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}

QWidget* MetadataPanel::previewWidget() const
{
    return m_preview;
}

QWidget* MetadataPanel::metadataWidget() const
{
    return m_metadata;
}

QImage MetadataPanel::loadImage(const QString& path, const QByteArray& fileData) const
{
    const QString lower = path.toLower();
    if (lower.endsWith("heic") || lower.endsWith("heif"))
    {
        return HeifPreview::thumbnail(fileData, 512);
    }
    return JpegRepair::read(fileData, true);
}

bool MetadataPanel::event(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        m_metadata->setHeaderLabels({ tr("Property"), tr("Value") });
        setFileData(m_path, m_data);   // the row labels are translated, so rebuild them
    }
    return QObject::event(event);
}

void MetadataPanel::setFile(const QString& path)
{
    QByteArray fileData;
    QFile      file(path);
    if (!path.isEmpty() && file.open(QIODevice::ReadOnly))
    {
        fileData = file.readAll();
    }
    setFileData(path, fileData);
}

void MetadataPanel::setFileData(const QString& path, const QByteArray& fileData)
{
    m_path = path;
    m_data = fileData;
    m_metadata->clear();
    if (path.isEmpty())
    {
        m_preview->clear();
        return;
    }

    // Preview (scaled).
    QImage img = loadImage(path, fileData);
    if (!img.isNull())
    {
        const QSize box = m_preview->size().isValid() && m_preview->width() > 20
                              ? m_preview->size()
                              : QSize(240, 240);
        m_preview->setPixmap(QPixmap::fromImage(
            img.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
    else
    {
        m_preview->setText(tr("No preview"));
    }

    // Metadata.
    auto add = [this](const QString& k, const QString& v)
    {
        if (!v.isEmpty())
        {
            new QTreeWidgetItem(m_metadata, { k, v });
        }
    };
    QFileInfo fi(path);
    add(tr("Name"), fi.fileName());
    add(tr("Size"), QString::number(fi.size() / 1024) + " KB");
    add(tr("Modified"), fi.lastModified().toString(Qt::ISODate));
    // The HEIF preview is a thumbnail, so its size is not the photo's.
    const QString lower = path.toLower();
    const QSize   dims  = lower.endsWith("heic") || lower.endsWith("heif") ? HeifPreview::size(fileData) : img.size();
    if (!dims.isEmpty())
    {
        add(tr("Dimensions"), QString("%1 x %2").arg(dims.width()).arg(dims.height()));
    }

    if (!fileData.isEmpty())
    {
        const qsizetype    head = qMin<qsizetype>(fileData.size(), 256 * 1024);   // header is enough for EXIF
        TinyEXIF::EXIFInfo exif(reinterpret_cast<const uint8_t*>(fileData.constData()), unsigned(head));
        if (exif.Fields)
        {
            const QString make  = QString::fromStdString(exif.Make);
            const QString model = QString::fromStdString(exif.Model);
            add(tr("Camera"), (make + " " + model).trimmed());
            add(tr("Date"), QString::fromStdString(exif.DateTimeOriginal));
            if (exif.ExposureTime > 0)
            {
                add(tr("Exposure"), QString("1/%1 s").arg(qRound(1.0 / exif.ExposureTime)));
            }
            if (exif.FNumber > 0)
            {
                add(tr("F-number"), "f/" + QString::number(exif.FNumber, 'g', 3));
            }
            if (exif.ISOSpeedRatings > 0)
            {
                add(tr("ISO"), QString::number(exif.ISOSpeedRatings));
            }
            if (exif.FocalLength > 0)
            {
                add(tr("Focal length"), QString::number(exif.FocalLength, 'g', 3) + " mm");
            }
            if (exif.GeoLocation.hasLatLon())
            {
                add(tr("GPS"), QString("%1, %2")
                                   .arg(exif.GeoLocation.Latitude)
                                   .arg(exif.GeoLocation.Longitude));
            }
        }
    }
}
