#ifndef SEFREADER_H
#define SEFREADER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Samsung Extended Format: a container appended after the JPEG EOI, so it is
// neither EXIF nor XMP and TinyEXIF never sees it. Layout:
//   ... <blocks> "SEFH" <u32 version><u32 count> <entries[12B]...> <u32 dirLen> "SEFT"
// Each entry is [u16 unk][u16 type][u32 offsetBack][u32 blockLen] with
// blockStart = SEFH - offsetBack; each block is
//   [u16 unk][u16 type][u32 nameLen][name][payload].
namespace Sef
{
    struct Block
    {
        std::string name;
        size_t      offset = 0;   // payload start, absolute in the file
        size_t      length = 0;   // payload length
        uint16_t    type   = 0;
    };

    // Requires the complete file: the directory sits at the very end. Returns the
    // blocks in directory order, or empty when there is no trailer. Every offset
    // and length in the result is guaranteed to lie inside [data, data + size).
    std::vector<Block> parse(const uint8_t* data, size_t size);

    // True if the block payload begins with an ISO-BMFF box header, i.e. a clip.
    bool isIsoBmff(const uint8_t* data, size_t size, const Block& block);
}

#endif // SEFREADER_H
