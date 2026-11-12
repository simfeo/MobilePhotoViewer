#include "jpegrepair.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>

namespace
{

    // Offset of the first marker after the scan that is neither a stuffed zero nor a
    // restart marker -- i.e. where the entropy-coded data ends. -1 if there is no scan.
    qint64 scanEnd(const QByteArray& data)
    {
        const uchar* p = reinterpret_cast<const uchar*>(data.constData());
        const qint64 n = data.size();
        qint64       i = 2;
        while (i < n - 1)
        {
            if (p[i] != 0xFF)
            {
                ++i;
                continue;
            }
            const uchar marker = p[i + 1];
            if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
            {
                i += 2;
                continue;
            }
            if (i + 4 > n)
            {
                break;
            }
            const int segment = (p[i + 2] << 8) | p[i + 3];
            if (segment < 2)
            {
                break;
            }
            if (marker == 0xDA)
            {
                for (qint64 j = i + 2 + segment; j < n - 1; ++j)
                {
                    if (p[j] == 0xFF && p[j + 1] != 0x00 && !(p[j + 1] >= 0xD0 && p[j + 1] <= 0xD7))
                    {
                        return j;
                    }
                }
                return n;
            }
            i += 2 + segment;
        }
        return -1;
    }

    void applyBox(QImageReader& reader, int box)
    {
        if (box <= 0)
        {
            return;
        }
        QSize size = reader.size();
        if (!size.isValid())
        {
            return;
        }
        size.scale(box, box, Qt::KeepAspectRatio);
        reader.setScaledSize(size);
    }

}   // namespace

QImage JpegRepair::read(const QString& path, bool autoTransform, int box)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QImage();
    }
    return read(file.readAll(), autoTransform, box);
}

QImage JpegRepair::read(const QByteArray& fileData, bool autoTransform, int box)
{
    QBuffer buffer;
    buffer.setData(fileData);   // implicitly shared, not copied
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(autoTransform);
    applyBox(reader, box);
    QImage image = reader.read();
    if (!image.isNull())
    {
        return image;
    }

    const qint64 end = scanEnd(fileData);
    if (end <= 0)
    {
        return image;
    }

    QByteArray data = fileData.left(end);
    data.append("\xFF\xD9", 2);   // terminate the stream libjpeg ran off the end of

    QBuffer repairedBuffer(&data);
    repairedBuffer.open(QIODevice::ReadOnly);
    QImageReader repaired(&repairedBuffer, "jpeg");
    repaired.setAutoTransform(autoTransform);
    applyBox(repaired, box);
    return repaired.read();
}
