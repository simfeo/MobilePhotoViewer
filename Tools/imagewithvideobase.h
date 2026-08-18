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

    // Motion-photo clips are framed like the still and carry their own rotation.
    // Sweep footage (panorama, wide selfie) is raw 16:9 capture with neither, so
    // it has to be letterboxed and turned to match the still.
    bool VideoFillsStill() const;
    int  VideoExtraRotation() const;

    // Samsung sweep panoramas carry no GPano geometry, only a container marker.
    bool IsPanoramaShot() const;


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
    bool                        m_videoFillsStill = true;
    int                         m_videoExtraRotation = 0;
    bool                        m_panoramaShot = false;
};

#endif // IMAGEWITHVIDEOBASE_H
