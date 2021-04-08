#include "jpeginmemoryvideo.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}


namespace JpegUtils {

    InMemoryVid::InMemoryVid(uint8_t* buf, size_t len)
        : m_length(len)
        , m_currentPos(0)
    {
        m_bufferVec.resize(len);
        memcpy_s((void*)(&m_bufferVec[0]), len, buf, len);
        m_buffer = &m_bufferVec[0];
    }

    int InMemoryVid::FileStreamRead(uint8_t* buf, int buf_size)
    {
        if (m_currentPos < static_cast<int64_t>(m_length))
        {
            size_t bytesRead = buf_size;
            if (bytesRead > (m_length - m_currentPos))
            {
                bytesRead = m_length - m_currentPos;
            }
            memcpy(buf, m_buffer + m_currentPos, bytesRead);
            m_currentPos += buf_size;
            return static_cast<int>(bytesRead);
        }
        return -1;
    }

    int InMemoryVid::setPosition(int64_t newPos)
    {
        if (newPos >= 0 && newPos <= static_cast<int64_t>(m_length))
        {
            m_currentPos = newPos;
            return 0;
        }
        return -1;
    }

    int64_t InMemoryVid::getPosition()
    {
        return m_currentPos;
    }

    size_t InMemoryVid::getSize()
    {
        return m_length;
    }


    int FileStreamRead(void* ptr, uint8_t* buf, int buf_size)
    {
        // This is your IStream
        InMemoryVid* stream = reinterpret_cast<InMemoryVid*>(ptr);

        int bytesRead = stream->FileStreamRead(buf, buf_size);

        if (bytesRead == -1)
        {
            return AVERROR_EOF;  // End of file
        }

        return bytesRead;
    }

    int64_t FileStreamSeek(void* ptr, int64_t offset, int whence)
    {
        InMemoryVid* stream = reinterpret_cast<InMemoryVid*>(ptr);

        switch (whence)
        {
        case SEEK_SET:
            return stream->setPosition(offset) != -1 ? stream->getPosition() : -1;
        case SEEK_CUR:
            return stream->setPosition(stream->getPosition() + offset) != -1 ? stream->getPosition() : -1;
        case SEEK_END:
            return stream->setPosition(stream->getSize() + offset) != -1 ? stream->getPosition() : -1;
        case AVSEEK_SIZE:
        {
            // Special whence for determining filesize without any seek.
            return stream->getSize();
        }
        break;
        }
        return -1;
    }

}
