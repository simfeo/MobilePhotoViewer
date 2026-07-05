#include "jpegmanager.h"

#include <QDebug>
#include <QImage>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
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

    // Fallback for older Samsung motion photos that carry no GCamera XMP metadata:
    // the MP4 is stored in a Samsung Extended Format (SEF) block tagged
    // "MotionPhoto_Data". The SEF directory sits at the very end of the file as
    //   ... "SEFH" <entries> <uint32 dirLen> "SEFT"
    // so the video's end boundary is derived from the trailer (not searched), and
    // its start is right after the tag (the MP4 immediately follows the tag name).
    if (!m_memoryVideo && data.size() >= 8 && std::memcmp(&data[data.size() - 4], "SEFT", 4) == 0)
    {
        const size_t n = data.size();
        const size_t p = n - 8;
        const uint32_t dirLen = static_cast<uint32_t>(data[p]) |
                               (static_cast<uint32_t>(data[p + 1]) << 8) |
                               (static_cast<uint32_t>(data[p + 2]) << 16) |
                               (static_cast<uint32_t>(data[p + 3]) << 24);
        if (static_cast<uint64_t>(dirLen) + 8 <= n)
        {
            const size_t sefhPos = n - 8 - dirLen;   // start of SEF directory == end of the video
            if (std::memcmp(&data[sefhPos], "SEFH", 4) == 0)
            {
                static const char kTag[] = "MotionPhoto_Data";
                const size_t tagLen = sizeof(kTag) - 1;
                auto tag = std::search(data.begin(), data.begin() + sefhPos, kTag, kTag + tagLen);
                const size_t mp4Start = static_cast<size_t>(std::distance(data.begin(), tag)) + tagLen;
                if (tag != data.begin() + sefhPos && sefhPos > mp4Start)
                {
                    qDebug() << "Samsung SEF motion photo: mp4 bytes [" << mp4Start << ".." << sefhPos << "]";
                    m_memoryVideo.reset(new QBuffer());
                    m_memoryVideo->setData(reinterpret_cast<char*>(data.data() + mp4Start),
                                           static_cast<int>(sefhPos - mp4Start));
                }
            }
        }
    }

    /*
     *
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
}

}
