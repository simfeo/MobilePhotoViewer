#ifndef JPEGINMEMORYVIDEO_H
#define JPEGINMEMORYVIDEO_H

#include <stdint.h>
#include <vector>

namespace JpegUtils
{
    class InMemoryVid
    {
    public:
        InMemoryVid(uint8_t* buf, size_t len);

        int FileStreamRead(uint8_t* buf, int buf_size);

        int setPosition(int64_t newPos);

        int64_t getPosition();

        size_t getSize();

    private:
        size_t                  m_length;
        int64_t                 m_currentPos;
        uint8_t* m_buffer;
        std::vector<uint8_t>    m_bufferVec;
    };

    int FileStreamRead(void* ptr, uint8_t* buf, int buf_size);

    int64_t FileStreamSeek(void* ptr, int64_t offset, int whence);
}
#endif // JPEGINMEMORYVIDEO_H
