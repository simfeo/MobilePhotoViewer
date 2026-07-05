#include "jpegmanager.h"

#include <QDebug>
#include <QImage>

#include <iostream>
#include <fstream>
#include <cstring>


JpegManager::JpegManager(const QString& imgPath)
    : m_imagePath(imgPath)
{
    manageJpeg();
}

void JpegManager::manageJpeg()
{
    std::shared_ptr<std::ifstream> imgFile(new std::ifstream(m_imagePath.toStdString().c_str(), std::ifstream::in | std::ifstream::binary), [](std::ifstream* p) {if (p) { p->close(), delete p; }});
    imgFile->seekg(0, std::ios::end);
    std::streampos length = imgFile->tellg();
    imgFile->seekg(0, std::ios::beg);
    std::vector<uint8_t> data(length);
    imgFile->read((char*)data.data(), length);

    m_imageEXIF = TinyEXIF::EXIFInfo(data.data(), length);
    if (m_imageEXIF.Fields)
    {
        qDebug() << "Image Description " << m_imageEXIF.ImageDescription.c_str() << "\n" \
                 << "Image Resolution " << m_imageEXIF.ImageWidth << "x" << m_imageEXIF.ImageHeight << " pixels\n" \
                 << "Camera Model " << m_imageEXIF.Make.c_str() << " - " << m_imageEXIF.Model.c_str() << "\n" \
                 << "Focal Length " << m_imageEXIF.FocalLength << " mm";
        if (m_imageEXIF.MicroVideo.HasMicroVideo)
        {
            qDebug() << "microvideo offset " << m_imageEXIF.MicroVideo.MicroVideoOffset;
            size_t offset = data.size() - m_imageEXIF.MicroVideo.MicroVideoOffset;
            m_memoryVideo.reset(new QBuffer());
            m_memoryVideo->setData(reinterpret_cast<char *>(data.data() + offset), data.size() - offset);
        }
    }

    // Fallback for Samsung motion photos with no GCamera XMP metadata. The clip is
    // appended in a Samsung Extended Format (SEF) block, indexed by a directory at
    // the end of the file:
    //   ... <blocks> "SEFH" <u32 version><u32 count> <entries[12B]...> <u32 dirLen> "SEFT"
    // Each entry is [u16 unk][u16 type][u32 offsetBack][u32 blockLen] with
    // blockStart = SEFH - offsetBack; each block is [u16 unk][u16 type][u32 nameLen][name][data].
    // The video is the block whose data begins with an ISO-BMFF 'ftyp' box -- that
    // covers MotionPhoto / Motion-Panorama and mp4 / 3gp alike, without depending on
    // the (varying) block name.
    if (!m_memoryVideo && data.size() >= 12 && std::memcmp(&data[data.size() - 4], "SEFT", 4) == 0)
    {
        const size_t n = data.size();
        auto u32 = [&data](size_t at) {
            return  static_cast<uint32_t>(data[at])          | (static_cast<uint32_t>(data[at + 1]) << 8)
            | (static_cast<uint32_t>(data[at + 2]) << 16) | (static_cast<uint32_t>(data[at + 3]) << 24);
        };
        const uint32_t dirLen = u32(n - 8);
        if (static_cast<uint64_t>(dirLen) + 8 <= n)
        {
            const size_t sefh = n - 8 - dirLen;
            if (std::memcmp(&data[sefh], "SEFH", 4) == 0)
            {
                const uint32_t count = u32(sefh + 8);
                const size_t dirEnd = n - 8;
                size_t entry = sefh + 12;
                for (uint32_t i = 0; i < count && entry + 12 <= dirEnd; ++i, entry += 12)
                {
                    const uint32_t offBack  = u32(entry + 4);
                    const uint32_t blockLen = u32(entry + 8);
                    if (offBack > sefh || blockLen < 8)
                        continue;
                    const size_t blockStart = sefh - offBack;
                    const size_t blockEnd   = blockStart + blockLen;
                    if (blockStart + 8 > n || blockEnd > n)
                        continue;
                    const uint32_t nameLen  = u32(blockStart + 4);
                    const size_t videoStart = blockStart + 8 + nameLen;
                    if (videoStart + 8 > blockEnd)
                        continue;
                    if (std::memcmp(&data[videoStart + 4], "ftyp", 4) == 0)  // ISO-BMFF video
                    {
                        qDebug() << "Samsung SEF motion photo: video bytes [" << videoStart << ".." << blockEnd << "]";
                        m_memoryVideo.reset(new QBuffer());
                        m_memoryVideo->setData(reinterpret_cast<char*>(data.data() + videoStart),
                                               static_cast<int>(blockEnd - videoStart));
                        break;
                    }
                }
            }
        }
    }
    /*
    // this is like brute force
    if (!m_memoryVideo)
    {
        static const char kMarker[] = "MotionPhoto_Data";
        auto marker = std::search(data.begin(), data.end(), kMarker, kMarker + (sizeof(kMarker) - 1));
        if (marker != data.end())
        {
            static const char kFtyp[] = "ftyp";
            auto ftyp = std::search(marker, data.end(), kFtyp, kFtyp + 4);
            if (ftyp != data.end() && std::distance(data.begin(), ftyp) >= 4)
            {
                const size_t mp4Start = static_cast<size_t>(std::distance(data.begin(), ftyp)) - 4; // box-size prefix
                static const char kSefh[] = "SEFH";
                auto sefh = std::search(data.begin() + mp4Start, data.end(), kSefh, kSefh + 4);
                const size_t mp4End = (sefh != data.end())
                                          ? static_cast<size_t>(std::distance(data.begin(), sefh))
                                          : data.size();
                if (mp4End > mp4Start)
                {
                    qDebug() << "Samsung SEF motion photo: mp4 bytes [" << mp4Start << ".." << mp4End << "]";
                    m_memoryVideo.reset(new QBuffer());
                    m_memoryVideo->setData(reinterpret_cast<char*>(data.data() + mp4Start),
                                           static_cast<int>(mp4End - mp4Start));
                }
            }
        }
    }
    */
}
