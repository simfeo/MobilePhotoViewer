#include "imagewithvideobase.h"

ImageWithVideoBase::ImageWithVideoBase():m_memoryVideo(nullptr)
{}


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
