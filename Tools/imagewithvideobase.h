#ifndef IMAGEWITHVIDEOBASE_H
#define IMAGEWITHVIDEOBASE_H

#include <QBuffer>
#include <QImage>

class ImageWithVideoBase
{
public:
    ImageWithVideoBase();

    bool HasMicrovideo();
    std::shared_ptr<QBuffer> getVideoMemory();

    // 8-bit grayscale, white nearest the camera; null when the shot has no depth.
    bool HasDepthMap() const;
    QImage getDepthMap() const;


    virtual size_t getWidth() const
    {
        return 0;
    }

    virtual size_t getHeight() const
    {
        return 0;
    }

    virtual std::shared_ptr<uint8_t> getDecodedImage()
    {
        return nullptr;
    }

protected:
    std::shared_ptr<QBuffer>    m_memoryVideo;
    QImage                      m_depthMap;
};

#endif // IMAGEWITHVIDEOBASE_H
