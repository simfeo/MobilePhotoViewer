#include "jpegmanager.h"

#include <QDebug>
#include <QImage>

#include <iostream>
#include <fstream>
#include <string_view>
#include <span>
#include <algorithm>  // std::ranges::equal
#include <cmath>
#include <QTransform>

// flipped() exists only from Qt 6.9; the project still targets 6.5.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#  define MPV_FLIP(img, h, v) ((img).flipped((h ? Qt::Horizontal : Qt::Orientations()) \
                                           | (v ? Qt::Vertical   : Qt::Orientations())))
#else
#  define MPV_FLIP(img, h, v) ((img).mirrored(h, v))
#endif


JpegManager::JpegManager(const QString& imgPath)
    : m_imagePath(imgPath)
{
    manageJpeg();
}

namespace {
// The DualShot depth block stores no geometry, so the dimensions are recovered by
// factorising its length and matching the still's aspect (1024x576 on the S22).
QImage depthFromRaw(const uint8_t* bytes, size_t len, double imageAspect)
{
    if (len == 0 || len > (16u << 20))
        return QImage();

    int bestW = 0, bestH = 0;
    double bestErr = 1e9;
    for (size_t w = 16; w <= len / 16; ++w)
    {
        if (len % w)
            continue;
        const size_t h = len / w;
        const double err = std::abs(double(w) / double(h) - imageAspect);
        if (err < bestErr)
        {
            bestErr = err;
            bestW = int(w);
            bestH = int(h);
        }
    }
    // A real depth map matches the still closely; reject anything else.
    if (bestW == 0 || bestErr > 0.02)
        return QImage();

    QImage depth(reinterpret_cast<const uchar*>(bytes), bestW, bestH, bestW,
                 QImage::Format_Grayscale8);
    // Samsung stores distance (bright = far); the viewers want disparity.
    depth = depth.copy();
    depth.invertPixels();
    return depth;
}

// Depth is stored in sensor orientation; the still is shown through
// setAutoTransform, so it needs the same rotation or portrait shots disagree.
// Sweep clips are stored sideways with an identity display matrix, whatever the
// still's EXIF says, so the angle cannot be read from the file. Measured: all 8
// panoramas in the library need 90 clockwise. Only one wide-selfie clip exists
// to measure, so that 270 may not hold for every capture.
int rotationForClip(std::string_view blockName)
{
    if (blockName.starts_with("Motion_Panorama"))
        return 90;
    if (blockName.starts_with("Wide_Selfie"))
        return 270;
    return 0;
}

QImage applyExifOrientation(const QImage& img, uint32_t orientation)
{
    QTransform t;
    switch (orientation)
    {
    case 2: return MPV_FLIP(img, true, false);
    case 3: t.rotate(180); break;
    case 4: return MPV_FLIP(img, false, true);
    case 5: t.rotate(90);  return MPV_FLIP(img, true, false).transformed(t);
    case 6: t.rotate(90);  break;
    case 7: t.rotate(270); return MPV_FLIP(img, true, false).transformed(t);
    case 8: t.rotate(270); break;
    default: return img;   // 1, or absent/unknown
    }
    return img.transformed(t);
}
}   // namespace

void JpegManager::manageJpeg()
{
    std::shared_ptr<std::ifstream> imgFile(new std::ifstream(m_imagePath.toStdString().c_str(), std::ifstream::in | std::ifstream::binary), [](std::ifstream* p) {if (p) { p->close(), delete p; }});
    imgFile->seekg(0, std::ios::end);
    std::streampos length = imgFile->tellg();
    imgFile->seekg(0, std::ios::beg);
    std::vector<uint8_t> data(length);
    imgFile->read((char*)data.data(), length);

    // True if the 4-byte magic tag sits at data[pos..pos+len), bounds-checked.
    auto magicAt = [&data](size_t pos, std::string_view tag) {
        return pos + tag.size() <= data.size()
            && std::ranges::equal(std::span(data.data() + pos, tag.size()), tag,
                                  [](uint8_t a, char b) { return a == static_cast<uint8_t>(b); });
    };

    m_imageEXIF = TinyEXIF::EXIFInfo(data.data(), length);
    if (m_imageEXIF.Fields)
    {
        qDebug() << "Image Description " << m_imageEXIF.ImageDescription.c_str() << "\n" \
                 << "Image Resolution " << m_imageEXIF.ImageWidth << "x" << m_imageEXIF.ImageHeight << " pixels\n" \
                 << "Camera Model " << m_imageEXIF.Make.c_str() << " - " << m_imageEXIF.Model.c_str() << "\n" \
                 << "Focal Length " << m_imageEXIF.FocalLength << " mm";
        // The video length (MicroVideoOffset, measured from the end of the file)
        // must fit inside the file. Some files carry the MotionPhoto metadata but
        // no actual clip (stripped) -- guarding here avoids a size_t underflow and
        // an out-of-bounds read.
        const uint64_t vlen = m_imageEXIF.MicroVideo.MicroVideoOffset;
        if (m_imageEXIF.MicroVideo.HasMicroVideo && vlen > 8 && vlen <= data.size())
        {
            const size_t offset = data.size() - static_cast<size_t>(vlen);
            if (magicAt(offset + 4, "ftyp"))   // real ISO-BMFF clip
            {
                qDebug() << "microvideo offset " << m_imageEXIF.MicroVideo.MicroVideoOffset;
                m_memoryVideo.reset(new QBuffer());
                m_memoryVideo->setData(reinterpret_cast<char*>(data.data() + offset),
                                       static_cast<int>(data.size() - offset));
            }
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
    if (data.size() >= 12 && magicAt(data.size() - 4, "SEFT"))
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
            if (magicAt(sefh, "SEFH"))
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
                    const size_t payloadStart = blockStart + 8 + nameLen;
                    if (payloadStart + 8 > blockEnd || blockStart + 8 + nameLen > n)
                        continue;
                    const std::string_view name(reinterpret_cast<const char*>(data.data() + blockStart + 8),
                                                nameLen);

                    if (name.starts_with("Panorama_Shot_Info") || name.starts_with("Motion_Panorama"))
                        m_panoramaShot = true;

                    // Live Focus shots carry the depth map beside the stills.
                    if (m_depthMap.isNull() && name.starts_with("DualShot_DepthMap"))
                    {
                        const double aspect = m_imageEXIF.ImageHeight
                            ? double(m_imageEXIF.ImageWidth) / double(m_imageEXIF.ImageHeight) : 0.0;
                        m_depthMap = depthFromRaw(data.data() + payloadStart,
                                                  blockEnd - payloadStart, aspect);
                        if (!m_depthMap.isNull())
                        {
                            m_depthMap = applyExifOrientation(m_depthMap, m_imageEXIF.Orientation);
                            qDebug() << "Samsung SEF depth map:" << m_depthMap.size()
                                     << "orientation" << m_imageEXIF.Orientation;
                        }
                        continue;
                    }

                    if (!m_memoryVideo && magicAt(payloadStart + 4, "ftyp"))  // ISO-BMFF video
                    {
                        qDebug() << "Samsung SEF clip:" << QString::fromUtf8(name.data(), int(name.size()))
                                 << "bytes [" << payloadStart << ".." << blockEnd << "]";
                        m_memoryVideo.reset(new QBuffer());
                        m_memoryVideo->setData(reinterpret_cast<char*>(data.data() + payloadStart),
                                               static_cast<int>(blockEnd - payloadStart));
                        // Only MotionPhoto clips share the still's framing.
                        m_videoFillsStill = name.starts_with("MotionPhoto");
                        if (!m_videoFillsStill)
                            m_videoExtraRotation = rotationForClip(name);
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
