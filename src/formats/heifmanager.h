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
    // fileData is the whole file, already read by the caller; nothing here touches the disk.
    explicit HeifManager(const QByteArray& fileData);

    size_t                   getWidth() const override;
    size_t                   getHeight() const override;
    std::shared_ptr<uint8_t> getDecodedImage() override;

    // The decoded pixels without a copy; the image keeps them alive on its own.
    QImage image() const;

private:
    void manageHeif(const QByteArray& fileData);

    uint32_t                 m_width;
    uint32_t                 m_height;
    uint32_t                 m_stride;
    std::shared_ptr<uint8_t> m_decodedImage;
    TinyEXIF::EXIFInfo       m_imageEXIF;
};

#endif   // HEIFMANAGER_H
