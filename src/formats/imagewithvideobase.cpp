#include "imagewithvideobase.h"

#include <QTransform>

// flipped() exists only from Qt 6.9; the project still targets 6.5.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#define MPV_FLIP(img, h, v) ((img).flipped((h ? Qt::Horizontal : Qt::Orientations()) | (v ? Qt::Vertical : Qt::Orientations())))
#else
#define MPV_FLIP(img, h, v) ((img).mirrored(h, v))
#endif

ImageWithVideoBase::ImageWithVideoBase()
    : m_memoryVideo(nullptr)
{
}

bool ImageWithVideoBase::HasMicrovideo()
{
    return m_memoryVideo != nullptr;
}

std::shared_ptr<QBuffer> ImageWithVideoBase::getVideoMemory()
{
    return m_memoryVideo;
}

bool ImageWithVideoBase::HasDepthMap() const
{
    return !m_depthMap.isNull();
}

QImage ImageWithVideoBase::getDepthMap() const
{
    return m_depthMap;
}

bool ImageWithVideoBase::VideoFillsStill() const
{
    return m_videoFillsStill;
}

int ImageWithVideoBase::VideoExtraRotation() const
{
    return m_videoExtraRotation;
}

bool ImageWithVideoBase::IsPanoramaShot() const
{
    return m_panoramaShot;
}

int ImageWithVideoBase::AlternateFrameCount() const
{
    return int(m_altFrames.size());
}

QByteArray ImageWithVideoBase::GetAlternateFrame(int index) const
{
    if (index < 0 || index >= int(m_altFrames.size()))
    {
        return QByteArray();
    }
    return m_altFrames[size_t(index)];
}

uint32_t ImageWithVideoBase::StillOrientation() const
{
    return m_stillOrientation;
}

QImage ImageWithVideoBase::ApplyExifOrientation(const QImage& img, uint32_t orientation)
{
    QTransform t;
    switch (orientation)
    {
    case 2:
        return MPV_FLIP(img, true, false);
    case 3:
        t.rotate(180);
        break;
    case 4:
        return MPV_FLIP(img, false, true);
    case 5:
        t.rotate(90);
        return MPV_FLIP(img, true, false).transformed(t);
    case 6:
        t.rotate(90);
        break;
    case 7:
        t.rotate(270);
        return MPV_FLIP(img, true, false).transformed(t);
    case 8:
        t.rotate(270);
        break;
    default:
        return img;   // 1, or absent/unknown
    }
    return img.transformed(t);
}
