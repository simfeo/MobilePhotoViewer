#ifndef IMAGEWITHVIDEOBASE_H
#define IMAGEWITHVIDEOBASE_H

#include <QBuffer>
#include <QByteArray>
#include <QImage>

#include <cstdint>
#include <vector>

class ImageWithVideoBase
{
public:
    ImageWithVideoBase();

    bool                     HasMicrovideo();
    std::shared_ptr<QBuffer> getVideoMemory();

    // 8-bit grayscale, white nearest the camera; null when the shot has no depth.
    bool   HasDepthMap() const;
    QImage getDepthMap() const;

    // Motion-photo clips are framed like the still and carry their own rotation.
    // Sweep footage (panorama, wide selfie) is raw 16:9 capture with neither, so
    // it has to be letterboxed and turned to match the still.
    bool VideoFillsStill() const;
    int  VideoExtraRotation() const;

    // Samsung sweep panoramas carry no GPano geometry, only a container marker.
    bool IsPanoramaShot() const;

    // Full-resolution source frames the capture kept beside the still, as encoded
    // JPEG bytes. Empty for an ordinary photo.
    // EXIF Orientation of the still, 1 when absent. The source frames are stored
    // in sensor orientation, so they need this rather than their own tag: the
    // user may have rotated the file, which only rewrites the still's.
    uint32_t      StillOrientation() const;
    static QImage ApplyExifOrientation(const QImage& img, uint32_t orientation);

    int        AlternateFrameCount() const;
    QByteArray GetAlternateFrame(int index) const;

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
    std::shared_ptr<QBuffer> m_memoryVideo;
    QImage                   m_depthMap;
    bool                     m_videoFillsStill    = true;
    int                      m_videoExtraRotation = 0;
    bool                     m_panoramaShot       = false;
    std::vector<QByteArray>  m_altFrames;
    uint32_t                 m_stillOrientation = 1;
};

#endif   // IMAGEWITHVIDEOBASE_H
