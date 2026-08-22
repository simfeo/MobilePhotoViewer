#ifndef HEIFREADER_H
#define HEIFREADER_H

#include "heifboxes.h"

#include <fstream>
#include <vector>
#include <cstdint>   // std::int64_t (<stdint.h> only declares it globally)
#include <memory>

namespace HeifHelpers
{
    enum class OperationResult : int
    {
        Ok = 0,
        FILE_READ_ERROR,
        BAD_STREAM,
    };
}

class HeifReader
{
public:
    HeifReader();

    HeifHelpers::OperationResult load(const char* img_path);
    HeifHelpers::OperationResult load(std::ifstream& fstream);
    HeifHelpers::OperationResult readBoxParameters(std::ifstream& fstream, const std::int64_t fstream_size, std::string& boxType, std::int64_t& boxSize);
    SefdBox                      getSefdBox();
    size_t                       getSefdOffset();

    // Newer HEIF motion photos store the clip in a top-level 'mpvd' box whose
    // payload is a standalone MP4. These return its file offset and size (0 if none).
    size_t getMpvdVideoOffset() const;
    size_t getMpvdVideoSize() const;

private:
    HeifHelpers::OperationResult skipBox(std::ifstream& fstream);
    HeifHelpers::OperationResult handleSefd(std::ifstream& fstream);
    HeifHelpers::OperationResult handleMpvd(std::ifstream& fstream);
    HeifHelpers::OperationResult readBox(std::ifstream& fstream, std::vector<uint8_t>& bitstream);

    enum class ReaderState
    {
        UNINITIALIZED,
        INITIALIZING,
        READY
    };

    ReaderState m_readerState;
    size_t      m_streamLength;
    size_t      m_sefdOffset;
    SefdBox     m_sefd;
    size_t      m_mpvdOffset;
    size_t      m_mpvdSize;
};

#endif   // HEIFREADER_H
