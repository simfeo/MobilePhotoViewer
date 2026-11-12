#ifndef HEIFPREVIEW_H
#define HEIFPREVIEW_H

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

// Cheap HEIF previews from the thumbnail phones embed in the file, so the grid and
// the info panel never pay for a full decode of a 24 MP photo.
namespace HeifPreview
{
    // Fits a box x box square. With allowFullDecode false, a file without an embedded
    // thumbnail yields a null image instead of a full decode. The path overload reads
    // only what it needs from disk, which suits the grid; the other takes a file the
    // caller has already read.
    QImage thumbnail(const QString& path, int box, bool allowFullDecode = true);
    QImage thumbnail(const QByteArray& fileData, int box, bool allowFullDecode = true);

    // Displayed size of the primary image, read from the header without decoding.
    QSize size(const QByteArray& fileData);
}   // namespace HeifPreview

#endif   // HEIFPREVIEW_H
