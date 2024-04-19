#ifndef IMAGEWITHVIDEOBASE_H
#define IMAGEWITHVIDEOBASE_H

#include <QBuffer>

class ImageWithVideoBase
{
public:
    ImageWithVideoBase();

    bool HasMicrovideo();
    std::shared_ptr<QBuffer> getVideoMemory();


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
};

#endif // IMAGEWITHVIDEOBASE_H
