#include "sefreader.h"

#include <cstring>

namespace
{
uint32_t u32(const uint8_t* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint16_t u16(const uint8_t* p)
{
    return uint16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
}

bool magicAt(const uint8_t* data, size_t size, size_t pos, const char* tag)
{
    const size_t len = std::strlen(tag);
    return pos <= size && size - pos >= len && std::memcmp(data + pos, tag, len) == 0;
}
}   // namespace

namespace Sef
{

std::vector<Block> parse(const uint8_t* data, size_t size)
{
    std::vector<Block> blocks;
    if (!data || size < 12 || !magicAt(data, size, size - 4, "SEFT"))
        return blocks;

    const uint32_t dirLen = u32(data + size - 8);
    if (uint64_t(dirLen) + 8 > size)
        return blocks;

    const size_t sefh = size - 8 - dirLen;
    // The count sits at sefh+8, so the header needs 12 bytes, not the 4 that
    // matching "SEFH" would prove.
    if (size - sefh < 12 || !magicAt(data, size, sefh, "SEFH"))
        return blocks;

    const uint32_t count  = u32(data + sefh + 8);
    const size_t   dirEnd = size - 8;

    size_t entry = sefh + 12;
    for (uint32_t i = 0; i < count && entry + 12 <= dirEnd; ++i, entry += 12)
    {
        const uint32_t offBack  = u32(data + entry + 4);
        const uint32_t blockLen = u32(data + entry + 8);
        if (offBack > sefh || blockLen < 8)
            continue;

        const size_t blockStart = sefh - offBack;
        if (size - blockStart < blockLen)
            continue;
        const size_t blockEnd = blockStart + blockLen;

        const uint32_t nameLen = u32(data + blockStart + 4);
        if (nameLen > blockLen - 8)
            continue;

        Block b;
        b.type   = u16(data + blockStart + 2);
        b.offset = blockStart + 8 + nameLen;
        b.length = blockEnd - b.offset;
        b.name.assign(reinterpret_cast<const char*>(data + blockStart + 8), nameLen);
        // Names are padded to the recorded length, which would break equality tests.
        b.name.erase(b.name.find_last_not_of(std::string("\0 ", 2)) + 1);
        blocks.push_back(std::move(b));
    }
    return blocks;
}

bool isIsoBmff(const uint8_t* data, size_t size, const Block& block)
{
    return block.length >= 8 && magicAt(data, size, block.offset + 4, "ftyp");
}

}   // namespace Sef
