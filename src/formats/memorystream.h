#ifndef MEMORYSTREAM_H
#define MEMORYSTREAM_H

#include <cstddef>
#include <istream>
#include <streambuf>

// Read-only std::istream over bytes someone else owns, so stream-based parsers can
// share an already loaded file instead of opening it again.
class MemoryStreamBuf : public std::streambuf
{
public:
    MemoryStreamBuf(const char* data, size_t size)
    {
        char* begin = const_cast<char*>(data);   // the get area is never written through
        setg(begin, begin, begin + size);
    }

protected:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override
    {
        if (!(which & std::ios_base::in))
        {
            return pos_type(off_type(-1));
        }
        char* base   = dir == std::ios_base::beg ? eback() : dir == std::ios_base::cur ? gptr() : egptr();
        char* target = base + off;
        if (target < eback() || target > egptr())
        {
            return pos_type(off_type(-1));
        }
        setg(eback(), target, egptr());
        return pos_type(target - eback());
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override
    {
        return seekoff(off_type(pos), std::ios_base::beg, which);
    }
};

class MemoryStream : public std::istream
{
public:
    MemoryStream(const char* data, size_t size)
        : std::istream(nullptr), m_buf(data, size)
    {
        rdbuf(&m_buf);   // the buffer member is constructed after the istream base
    }

private:
    MemoryStreamBuf m_buf;
};

#endif   // MEMORYSTREAM_H
