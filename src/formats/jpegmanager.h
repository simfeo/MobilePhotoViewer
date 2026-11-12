#ifndef JPEGMANAGER_H
#define JPEGMANAGER_H

#include <QByteArray>

#include "imagewithvideobase.h"
#include <TinyEXIF.h>

class QImage;

namespace JpegUtils
{
    class InMemoryVid;
}

class JpegManager : public ImageWithVideoBase
{
public:
    // fileData is the whole file, already read by the caller; nothing here touches the disk.
    explicit JpegManager(const QByteArray& fileData);

private:
    void manageJpeg(const QByteArray& fileData);

    TinyEXIF::EXIFInfo m_imageEXIF;
};

#endif   // JPEGMANAGER_H
