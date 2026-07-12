#include "heifreader.h"

#include <Tools/ramfile.h>

#include <string>
#include <string_view>
#include <span>
#include <algorithm>  // std::ranges::equal

#include <QDebug>


namespace
{
    bool stream_can_be_read(std::ifstream& fstream)
    {
        char buffer;
        auto prev_pos = fstream.tellg();
        fstream.read(&buffer, sizeof(buffer));
        if (fstream && fstream.gcount() == 1)
        {
            fstream.seekg(prev_pos);
            return true;
        }
        else
        {
            return false;
        }
    }

    void readToBuffer(std::ifstream& fstream, char* buffer, size_t buffer_size)
    {
        fstream.read(buffer, buffer_size);
    }

    HeifHelpers::OperationResult readBytes(std::ifstream& fstream, int64_t count, std::int64_t& boxSize)
    {
        int64_t value = 0;
        for (unsigned int i = 0; i < count; ++i)
        {
            char buffer;
            fstream.read(&buffer, sizeof(buffer));

            value = (value << 8) | static_cast<int64_t>(static_cast<unsigned char>(buffer));
            if (!fstream || fstream.bad())
            {
                return HeifHelpers::OperationResult::FILE_READ_ERROR;
            }
        }

        boxSize = value;

        return HeifHelpers::OperationResult::Ok;
    }
}

HeifReader::HeifReader()
    : m_readerState(ReaderState::UNINITIALIZED)
    , m_streamLength(0)
    , m_sefdOffset(0)
    , m_sefd()
    , m_mpvdOffset(0)
    , m_mpvdSize(0)
{

}

HeifHelpers::OperationResult HeifReader::load(const char* img_path)
{
    std::shared_ptr<std::ifstream> imgFile(new std::ifstream(img_path, std::ifstream::in | std::ifstream::binary), [](std::ifstream* p) {if (p) { p->close(), delete p; }});
    return load(*imgFile.get());
}

HeifHelpers::OperationResult HeifReader::load(std::ifstream& fstream)
{
    if (!fstream || fstream.bad())
    {
        return HeifHelpers::OperationResult::BAD_STREAM;
    }

    ReaderState prevState = m_readerState;
    m_readerState = ReaderState::INITIALIZING;

    fstream.seekg(0, std::ios::beg);
    fstream.seekg(0, std::ios::end);
    std::streampos stream_end = fstream.tellg();
    fstream.seekg(0, std::ios::beg);
    m_streamLength = stream_end;


    bool sefd_found = false;
    HeifHelpers::OperationResult result = HeifHelpers::OperationResult::Ok;

    try
    {
        while ((result == HeifHelpers::OperationResult::Ok) && stream_can_be_read(fstream))
        {
            std::string boxType;
            std::int64_t boxSize = 0;
            //BitStream bitstream;
            result = readBoxParameters(fstream, m_streamLength, boxType, boxSize);
            if (result == HeifHelpers::OperationResult::Ok)
            {
                if (boxType == "ftyp"
                    || boxType == "etyp"
                    || boxType == "meta"
                    || boxType == "moov"
                    || boxType == "moof"
                    || boxType == "mdat"
                    || boxType == "free"
                    || boxType == "skip"
                    )
                {
                    result = skipBox(fstream);
                }

                else if (boxType == "sefd")
                {
                    result = handleSefd(fstream);
                    sefd_found = true;
                }
                else if (boxType == "mpvd")
                {
                    result = handleMpvd(fstream);
                }
                else
                {
                    qDebug() << "Skipping root level box of unknown type '" << boxType.c_str() << "'";
                    result = skipBox(fstream);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        qDebug() << "readStream std::exception Error:: " << e.what();
        result = HeifHelpers::OperationResult::FILE_READ_ERROR;
    }
    return result;
}

HeifHelpers::OperationResult HeifReader::readBoxParameters(std::ifstream& fstream, const std::int64_t fstream_size, std::string& boxType, std::int64_t& boxSize)
{
    const std::int64_t start_location = fstream.tellg();

    // Read the 32-bit length field of the box
    HeifHelpers::OperationResult result = readBytes(fstream, 4, boxSize);
    if (result != HeifHelpers::OperationResult::Ok)
    {
        return result;
    }

    // Read the four character string for boxType
    static const size_t BOX_LENGTH = 4;
    boxType.resize(BOX_LENGTH);
    readToBuffer(fstream, &boxType[0], BOX_LENGTH);
    if (!fstream || !fstream.good())
    {
        return HeifHelpers::OperationResult::FILE_READ_ERROR;
    }

    if (boxSize == 1)
    {
        result = readBytes(fstream, 8, boxSize);
        if (result != HeifHelpers::OperationResult::Ok)
        {
            return result;
        }
    }

    int64_t boxEndOffset = start_location + boxSize;
    if (boxSize < 8 || (boxEndOffset < 8) || (boxEndOffset > fstream_size))
    {
        return HeifHelpers::OperationResult::FILE_READ_ERROR;
    }

    // seek to box start
    fstream.seekg(start_location);
    if (!fstream || !fstream.good())
    {
        return HeifHelpers::OperationResult::FILE_READ_ERROR;
    }
    return HeifHelpers::OperationResult::Ok;
}

SefdBox HeifReader::getSefdBox()
{
    return m_sefd;
}

size_t HeifReader::getSefdOffset()
{
    return m_sefdOffset;
}

size_t HeifReader::getMpvdVideoOffset() const
{
    return m_mpvdOffset;
}

size_t HeifReader::getMpvdVideoSize() const
{
    return m_mpvdSize;
}


HeifHelpers::OperationResult HeifReader::skipBox(std::ifstream& fstream)
{
    const std::int64_t start_location = fstream.tellg();

    std::string boxType;
    std::int64_t boxSize = 0;
    HeifHelpers::OperationResult result = readBoxParameters(fstream, m_streamLength, boxType, boxSize);
    if (result != HeifHelpers::OperationResult::Ok)
    {
        return result;
    }

    fstream.seekg(start_location + boxSize);
    if (fstream.bad())
    {
        return HeifHelpers::OperationResult::FILE_READ_ERROR;
    }
    return HeifHelpers::OperationResult::Ok;
}


HeifHelpers::OperationResult HeifReader::handleSefd(std::ifstream& fstream)
{
    std::vector<uint8_t> boxDataRaw;
    m_sefdOffset = fstream.tellg();
    HeifHelpers::OperationResult result = readBox(fstream, boxDataRaw);
    std::shared_ptr<Utils::RamFile> boxData(new Utils::RamFile(boxDataRaw));

    if (result != HeifHelpers::OperationResult::Ok)
    {
        return result;
    }

    m_sefd = SefdBox(boxData);

    return HeifHelpers::OperationResult::Ok;
}

HeifHelpers::OperationResult HeifReader::handleMpvd(std::ifstream& fstream)
{
    // 'mpvd' (Motion Photo Video Data) box: the payload is a standalone MP4 file.
    // Record its offset/size so the caller can hand the clip straight to a player.
    const std::int64_t boxStart = fstream.tellg();

    std::int64_t boxSize = 0;
    HeifHelpers::OperationResult result = readBytes(fstream, 4, boxSize);   // 32-bit size
    if (result != HeifHelpers::OperationResult::Ok)
    {
        return result;
    }
    fstream.seekg(4, std::ios::cur);                                        // skip the 'mpvd' type

    size_t headerSize = 8;
    if (boxSize == 1)                                                       // 64-bit extended size
    {
        result = readBytes(fstream, 8, boxSize);
        if (result != HeifHelpers::OperationResult::Ok)
        {
            return result;
        }
        headerSize = 16;
    }
    else if (boxSize == 0)                                                  // extends to end of stream
    {
        boxSize = static_cast<std::int64_t>(m_streamLength) - boxStart;
    }

    if (boxSize < static_cast<std::int64_t>(headerSize) ||
        boxStart + boxSize > static_cast<std::int64_t>(m_streamLength))
    {
        return HeifHelpers::OperationResult::FILE_READ_ERROR;
    }

    const std::int64_t payloadOffset = boxStart + static_cast<std::int64_t>(headerSize);
    const std::int64_t payloadSize = boxSize - static_cast<std::int64_t>(headerSize);

    // Accept it only if the payload really is an ISO-BMFF clip (a '....ftyp' box).
    char magic[8] = { 0 };
    fstream.seekg(payloadOffset);
    fstream.read(magic, sizeof(magic));
    if (fstream.good() && payloadSize >= 8 && std::ranges::equal(std::span(magic + 4, 4), std::string_view("ftyp")))
    {
        m_mpvdOffset = static_cast<size_t>(payloadOffset);
        m_mpvdSize = static_cast<size_t>(payloadSize);
    }

    fstream.clear();                       // clear any eof/fail flags from the peek read
    fstream.seekg(boxStart + boxSize);     // advance past the box for the main loop
    if (fstream.bad())
    {
        return HeifHelpers::OperationResult::FILE_READ_ERROR;
    }
    return HeifHelpers::OperationResult::Ok;
}

HeifHelpers::OperationResult HeifReader::readBox(std::ifstream& fstream, std::vector<uint8_t>& bitstream)
{
    std::string boxType;
    std::int64_t boxSize = 0;
    HeifHelpers::OperationResult result = readBoxParameters(fstream, m_streamLength, boxType, boxSize);
    if (result != HeifHelpers::OperationResult::Ok)
    {
        return result;
    }

    bitstream.resize(static_cast<size_t>(boxSize));
    fstream.read(reinterpret_cast<char*>(&bitstream[0]), boxSize);
    if (fstream.bad())
    {
        return HeifHelpers::OperationResult::FILE_READ_ERROR;
    }

    return HeifHelpers::OperationResult::Ok;
}
