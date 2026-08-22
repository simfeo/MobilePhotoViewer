#ifndef HEIFMANAGER_H
#define HEIFMANAGER_H

#include <QString>
#include <memory>

#include "imagewithvideobase.h"
#include <TinyEXIF.h>

namespace JpegUtils
{
    class InMemoryVid;
}

class HeifManager : public ImageWithVideoBase
{
public:
    HeifManager(const QString& imgPath);

    void manageHeif();

    size_t                   getWidth() const override;
    size_t                   getHeight() const override;
    std::shared_ptr<uint8_t> getDecodedImage() override;

private:
    uint32_t                 m_width;
    uint32_t                 m_height;
    QString                  m_imagePath;
    std::shared_ptr<uint8_t> m_decodedImage;
    TinyEXIF::EXIFInfo       m_imageEXIF;
};

#endif   // HEIFMANAGER_H
