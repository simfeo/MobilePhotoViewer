#include "imagewithvideobase.h"

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
