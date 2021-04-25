#ifndef HEIFMANAGER_H
#define HEIFMANAGER_H

#include <QString>
#include <memory>
#include <tinyexif/TinyEXIF.h>

namespace JpegUtils
{
    class InMemoryVid;
}

class HeifManager
{
public:
    HeifManager(const QString& imgPath);

    void manageHeif();
    bool HasMicrovideo();
    std::shared_ptr<JpegUtils::InMemoryVid> getVideoMemory();

    size_t getWidth() const;
    size_t getHeight() const;
    std::shared_ptr<uint8_t> getDecodedImage();

private:
    uint32_t                                m_width;
    uint32_t                                m_height;
    QString                                 m_imagePath;
    std::shared_ptr<uint8_t>                m_decodedImage;
    std::shared_ptr<JpegUtils::InMemoryVid> m_memoryVideo;
    TinyEXIF::EXIFInfo                      m_imageEXIF;
};

#endif // HEIFMANAGER_H
