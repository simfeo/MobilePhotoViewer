#include "jpegmanager.h"

#include "sefreader.h"

#include <QDebug>
#include <QImage>

#include <iostream>
#include <string>
#include <string_view>
#include <span>
#include <algorithm>   // std::ranges::equal
#include <cmath>
#include <cstring>

JpegManager::JpegManager(const QByteArray& fileData)
{
    manageJpeg(fileData);
}

namespace
{
    uint32_t le32(const uint8_t* p)
    {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }

    // Owning 8-bit grayscale view of a raw plane.
    QImage grayFromRaw(const uint8_t* bytes, int w, int h)
    {
        if (w <= 0 || h <= 0)
        {
            return QImage();
        }
        return QImage(reinterpret_cast<const uchar*>(bytes), w, h, w, QImage::Format_Grayscale8).copy();
    }

    // DualShot_Extra_Info states the depth geometry: a marker of the form
    // <01..09> 00 ff ff is followed by width and height. The record starts with a
    // second, bogus marker, so the search begins past it (as ExifTool's does).
    QSize dualShotDepthSize(const uint8_t* extra, size_t len, size_t expected)
    {
        for (size_t i = 32; i + 12 <= len && i < 96; i += 4)
        {
            if (extra[i] < 0x01 || extra[i] > 0x09 || extra[i + 1] != 0x00 || extra[i + 2] != 0xFF || extra[i + 3] != 0xFF)
            {
                continue;
            }
            const uint32_t w = le32(extra + i + 4);
            const uint32_t h = le32(extra + i + 8);
            if (w && h && size_t(w) * h == expected)
            {
                return QSize(int(w), int(h));
            }
        }
        return QSize();
    }

    // Fallback when the geometry is missing: of the factorisations of 'len', take the
    // one closest to the still's aspect.
    QSize guessDepthSize(size_t len, double imageAspect)
    {
        if (len == 0 || len > (16u << 20))
        {
            return QSize();
        }

        int    bestW = 0, bestH = 0;
        double bestErr = 1e9;
        for (size_t w = 16; w <= len / 16; ++w)
        {
            if (len % w)
            {
                continue;
            }
            const size_t h   = len / w;
            const double err = std::abs(double(w) / double(h) - imageAspect);
            if (err < bestErr)
            {
                bestErr = err;
                bestW   = int(w);
                bestH   = int(h);
            }
        }
        // A real depth map matches the still closely; reject anything else.
        return (bestW && bestErr <= 0.02) ? QSize(bestW, bestH) : QSize();
    }

    // FocusShot_Map is two stacked half-resolution planes: a hard subject mask and a
    // soft depth map. FocusShot_Meta_Info ("FCSS") carries the still's size at +44.
    QSize focusShotPlaneSize(const uint8_t* meta, size_t metaLen, size_t mapLen)
    {
        if (metaLen < 52 || std::memcmp(meta, "FCSS", 4) != 0)
        {
            return QSize();
        }
        const int w = int(le32(meta + 44)) / 2;
        const int h = int(le32(meta + 48)) / 2;
        if (w <= 0 || h <= 0 || size_t(w) * h * 2 != mapLen)
        {
            return QSize();
        }
        return QSize(w, h);
    }

    // SingleShot is single-camera bokeh: one segmentation matte, no stereo. The
    // DOFS record names its geometry, so nothing has to be guessed.
    QSize singleShotPlaneSize(const uint8_t* data, size_t size, const Sef::Block& meta, size_t mapLen)
    {
        const std::vector<Sef::DofsField> fields = Sef::parseDofs(data, size, meta);
        int32_t                           w      = 0;
        int32_t                           h      = 0;
        if (!Sef::dofsInt(fields, "segWidth", w) || !Sef::dofsInt(fields, "segHeight", h))
        {
            return QSize();
        }
        if (w <= 0 || h <= 0 || size_t(w) * size_t(h) != mapLen)
        {
            return QSize();
        }
        return QSize(int(w), int(h));
    }

    // Fallback for sweep clips whose *_Motion_Info record is missing or unreadable.
    // Measured over the sample library; the record itself is preferred when present.
    int rotationForClip(std::string_view blockName)
    {
        if (blockName.starts_with("Motion_Panorama"))
        {
            return 90;
        }
        if (blockName.starts_with("Wide_Selfie"))
        {
            return 270;
        }
        return 0;
    }

}   // namespace

void JpegManager::manageJpeg(const QByteArray& fileData)
{
    const std::span<const uint8_t> data(reinterpret_cast<const uint8_t*>(fileData.constData()), size_t(fileData.size()));

    // True if the 4-byte magic tag sits at data[pos..pos+len), bounds-checked.
    auto magicAt = [&data](size_t pos, std::string_view tag)
    {
        return pos + tag.size() <= data.size() && std::ranges::equal(std::span(data.data() + pos, tag.size()), tag,
                                                                     [](uint8_t a, char b)
                                                                     { return a == static_cast<uint8_t>(b); });
    };

    m_imageEXIF        = TinyEXIF::EXIFInfo(data.data(), unsigned(data.size()));
    m_stillOrientation = m_imageEXIF.Orientation ? m_imageEXIF.Orientation : 1;
    if (m_imageEXIF.Fields)
    {
        qDebug() << "Image Description " << m_imageEXIF.ImageDescription.c_str() << "\n"
                 << "Image Resolution " << m_imageEXIF.ImageWidth << "x" << m_imageEXIF.ImageHeight << " pixels\n"
                 << "Camera Model " << m_imageEXIF.Make.c_str() << " - " << m_imageEXIF.Model.c_str() << "\n"
                 << "Focal Length " << m_imageEXIF.FocalLength << " mm";
        // Two spellings of the same idea: legacy GCamera:MicroVideo states an offset
        // back from the end of the file, the newer GCamera:MotionPhoto container
        // states the trailing item's length. Both put the clip at size - value.
        // The value must fit inside the file: some files keep the metadata but not
        // the clip (stripped), and that would underflow into an out-of-bounds read.
        uint64_t vlen = 0;
        if (m_imageEXIF.MicroVideo.HasMotionPhoto)
        {
            vlen = m_imageEXIF.MicroVideo.MotionPhotoLength;
        }
        else if (m_imageEXIF.MicroVideo.HasMicroVideo)
        {
            vlen = m_imageEXIF.MicroVideo.MicroVideoOffset;
        }

        if (vlen > 8 && vlen <= data.size())
        {
            const size_t offset = data.size() - static_cast<size_t>(vlen);
            if (magicAt(offset + 4, "ftyp"))   // real ISO-BMFF clip
            {
                qDebug() << "GCamera clip at" << offset << "length" << vlen
                         << QString::fromStdString(m_imageEXIF.MicroVideo.MotionPhotoMime);
                m_memoryVideo.reset(new QBuffer());
                m_memoryVideo->setData(reinterpret_cast<const char*>(data.data() + offset),
                                       static_cast<int>(data.size() - offset));
            }
        }
    }

    // Fallback for Samsung motion photos with no GCamera XMP metadata: the clip,
    // and any depth map, live in the SEF container appended after the JPEG.
    // NOT PART OF EXIF OR XMP STANDARD! THE BLOCK ARE IN THE VERY LAST OF THE FILE!
    const std::vector<Sef::Block> sefBlocks = Sef::parse(data.data(), data.size());
    if (!sefBlocks.empty())
    {
        const Sef::Block* depth = nullptr;
        const Sef::Block* meta  = nullptr;
        const Sef::Block* sweep = nullptr;
        std::string       clipName;
        bool              depthIsFocusShot  = false;
        bool              depthIsSingleShot = false;

        for (const Sef::Block& b : sefBlocks)
        {
            const std::string_view name(b.name);

            if (name.starts_with("Panorama_Shot_Info") || name.starts_with("Motion_Panorama"))
            {
                m_panoramaShot = true;
            }

            if (name == "Motion_Panorama_Info" || name == "Wide_Selfie_Motion_Info")
            {
                sweep = &b;
                continue;
            }

            // The map and the record describing it can appear in either order,
            // so the blocks are collected and resolved after the walk.
            // "SingeShot" is Samsung's own spelling of the block name.
            if (name.starts_with("DualShot_DepthMap") || name == "FocusShot_Map" || name.starts_with("SingeShot_DepthMap"))
            {
                depth             = &b;
                depthIsFocusShot  = (name == "FocusShot_Map");
                depthIsSingleShot = name.starts_with("SingeShot_DepthMap");
                continue;
            }
            if (name == "DualShot_Extra_Info" || name == "FocusShot_Meta_Info" || name == "SingleShot_Meta_Info")
            {
                meta = &b;
                continue;
            }

            // DualShot/FocusShot/SingleShot keep their source frames as whole JPEGs.
            if (Sef::isJpeg(data.data(), data.size(), b))
            {
                m_altFrames.push_back(
                    QByteArray(reinterpret_cast<const char*>(data.data() + b.offset), int(b.length)));
                continue;
            }

            if (!m_memoryVideo && Sef::isIsoBmff(data.data(), data.size(), b))
            {
                qDebug() << "Samsung SEF clip:" << QString::fromStdString(b.name)
                         << "bytes [" << b.offset << ".." << (b.offset + b.length) << "]";
                m_memoryVideo.reset(new QBuffer());
                m_memoryVideo->setData(reinterpret_cast<const char*>(data.data() + b.offset),
                                       static_cast<int>(b.length));
                // Only MotionPhoto clips share the still framing.
                m_videoFillsStill = name.starts_with("MotionPhoto");
                clipName          = b.name;
            }
        }

        if (!clipName.empty() && !m_videoFillsStill)
        {
            Sef::SweepInfo info;
            if (sweep && Sef::parseSweepInfo(data.data(), data.size(), *sweep, info))
            {
                m_videoExtraRotation = info.rotationDegrees;
                qDebug() << "Samsung SEF sweep:" << QString::fromStdString(sweep->name) << info.frameWidth << "x"
                         << info.frameHeight << info.frameCount << "frames, rotation" << info.rotationDegrees;
            }
            else
            {
                m_videoExtraRotation = rotationForClip(clipName);
            }
        }

        if (depth)
        {
            const uint8_t* map = data.data() + depth->offset;
            QSize          size;
            // The view wants disparity. DualShot stores distance and has to be
            // flipped; the segmentation mattes are already bright-is-near.
            bool brightIsNear = false;

            if (depthIsSingleShot)
            {
                size         = meta ? singleShotPlaneSize(data.data(), data.size(), *meta, depth->length) : QSize();
                brightIsNear = true;
            }
            else if (depthIsFocusShot)
            {
                // Selective Focus: a subject mask then a soft depth map,
                // the second of which is the useful one.
                size = meta ? focusShotPlaneSize(data.data() + meta->offset, meta->length, depth->length)
                            : QSize();
                if (size.isValid())
                {
                    map += size_t(size.width()) * size.height();   // skip the mask
                    brightIsNear = true;
                }
            }
            else
            {
                size = meta ? dualShotDepthSize(data.data() + meta->offset, meta->length, depth->length)
                            : QSize();
                if (!size.isValid())
                {
                    const double aspect = m_imageEXIF.ImageHeight
                                              ? double(m_imageEXIF.ImageWidth) / double(m_imageEXIF.ImageHeight)
                                              : 0.0;
                    size                = guessDepthSize(depth->length, aspect);
                }
            }

            if (size.isValid())
            {
                m_depthMap = grayFromRaw(map, size.width(), size.height());
                if (!brightIsNear)
                {
                    m_depthMap.invertPixels();   // stored as distance
                }
                m_depthMap = ApplyExifOrientation(m_depthMap, m_imageEXIF.Orientation);
                qDebug() << "Samsung SEF depth map:" << m_depthMap.size()
                         << (depthIsSingleShot  ? "(SingleShot)"
                             : depthIsFocusShot ? "(FocusShot)"
                                                : "(DualShot)")
                         << "orientation" << m_imageEXIF.Orientation;
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
                    m_memoryVideo->setData(reinterpret_cast<const char*>(data.data() + mp4Start),
                                           static_cast<int>(mp4End - mp4Start));
                }
            }
        }
    }
    */
}
