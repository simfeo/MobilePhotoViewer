#ifndef JPEGREPAIR_H
#define JPEGREPAIR_H

#include <QImage>
#include <QString>

// Reads a still, working around Samsung stills whose JPEG stream is not
// terminated: the scan is followed straight by the motion clip and the SEF
// blocks with no EOI marker, so libjpeg runs past the image and Qt returns
// nothing. Motion panoramas are written this way.
namespace JpegRepair
{
    // 'box' > 0 scales the decode down to fit a box x box square (thumbnails).
    QImage read(const QString& path, bool autoTransform, int box = 0);
}   // namespace JpegRepair

#endif   // JPEGREPAIR_H
