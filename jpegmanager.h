#ifndef JPEGMANAGER_H
#define JPEGMANAGER_H

#include <QString>
#include <memory>
#include <tinyexif/TinyEXIF.h>

class QImage;

namespace JpegUtils
{
    class InMemoryVid;
}

class JpegManager
{
public:
    JpegManager(const QString& imgPath);

    void manageJpeg();
    bool HasMicrovideo();
    std::shared_ptr<JpegUtils::InMemoryVid> getVideoMemory();

private:
    QString m_imagePath;
    TinyEXIF::EXIFInfo m_imageEXIF;
    std::shared_ptr<JpegUtils::InMemoryVid> m_memoryVideo;
};

#endif // JPEGMANAGER_H
