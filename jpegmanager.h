#ifndef JPEGMANAGER_H
#define JPEGMANAGER_H

#include <QString>

#include <Tools/imagewithvideobase.h>
#include <TinyEXIF.h>

class QImage;

namespace JpegUtils
{
    class InMemoryVid;
}

class JpegManager : public ImageWithVideoBase
{
public:
    JpegManager(const QString& imgPath);

    void manageJpeg();

private:
    QString                     m_imagePath;
    TinyEXIF::EXIFInfo          m_imageEXIF;
};

#endif // JPEGMANAGER_H
