#ifndef RAMFILE_H
#define RAMFILE_H

#include <cstdint>
#include <vector>

namespace Utils
{
    class RamFile
    {
    public:
        RamFile(std::vector<uint8_t>& invec);
        RamFile(uint8_t* buf, size_t len);

        int FileStreamRead(uint8_t* buf, int buf_size);
        int setPosition(int64_t newPos);
        int64_t getPosition();
        size_t getSize();

    private:
        size_t                  m_length;
        int64_t                 m_currentPos;
        uint8_t*                m_buffer;
        std::vector<uint8_t>    m_bufferVec;
    };

    int FileStreamRead(void* ptr, uint8_t* buf, int buf_size);
    int64_t FileStreamSeek(void* ptr, int64_t offset, int whence);
}
#endif // RAMFILE_H
