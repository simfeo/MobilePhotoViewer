#include "heifmanager.h"

#include <QDebug>
#include <QImage>

#include <libheif/heif.h>
#include <libheif/heif_cxx.h>

#include "heifdecode.h"
#include "heifreader.h"
#include "memorystream.h"

HeifManager::HeifManager(const QByteArray& fileData)
    : m_width(0), m_height(0), m_stride(0), m_decodedImage(nullptr)
{
    manageHeif(fileData);
}

void HeifManager::manageHeif(const QByteArray& fileData)
{
    const auto*  bytes = reinterpret_cast<const uint8_t*>(fileData.constData());
    const size_t size  = size_t(fileData.size());
    if (size < 12)
    {
        qDebug() << "Input file is too short for HEIF/AVIF\n";
        return;
    }
    heif_filetype_result filetype_check = heif_check_filetype(bytes, 12);
    if (filetype_check == heif_filetype_no)
    {
        qDebug() << "Input file is not an HEIF/AVIF file\n";
        return;
    }

    if (filetype_check == heif_filetype_yes_unsupported)
    {
        qDebug() << "Input file is an unsupported HEIF/AVIF file type\n";
        return;
    }

    std::shared_ptr<heif_context> ctx(heif_context_alloc(), [](heif_context* p)
                                      { heif_context_free(p); });
    if (!ctx)
    {
        qDebug() << "Could not create context object\n";
        return;
    }

    struct heif_error err;
    // libheif reads the tiles straight from the caller's buffer; fileData outlives ctx.
    err = heif_context_read_from_memory_without_copy(ctx.get(), bytes, size, nullptr);
    if (err.code != 0)
    {
        qDebug() << "Could not read HEIF/AVIF file: " << err.message;
        return;
    }

    struct heif_image_handle* handle = nullptr;
    err                              = heif_context_get_primary_image_handle(ctx.get(), &handle);
    if (err.code)
    {
        qDebug() << "Could not read the primary HEIF/AVIF image: " << err.message;
        return;
    }
    std::shared_ptr<heif_image_handle> handleGuard(handle, [](heif_image_handle* p)
                                                   { heif_image_handle_release(p); });

    if (heif_image_handle_get_luma_bits_per_pixel(handle) < 0)
    {
        qDebug() << "Input image has undefined bit-depth\n";
        return;
    }

    struct heif_image* image = nullptr;
    err = heifDecodeRGBA(handle, &image);
    if (err.code)
    {
        qDebug() << "Could not decode image: " << err.message;
        return;
    }
    std::shared_ptr<heif_image> img(image, [](heif_image* p)
                                    { heif_image_release(p); });

    int            r_stride  = 0;
    const uint8_t* img_plane = heif_image_get_plane_readonly(img.get(), heif_channel_interleaved, &r_stride);
    if (img_plane)
    {
        m_width  = heif_image_get_width(img.get(), heif_channel_interleaved);
        m_height = heif_image_get_height(img.get(), heif_channel_interleaved);
        m_stride = r_stride;
        // Shares ownership of the decoded heif_image instead of copying its pixels.
        m_decodedImage = std::shared_ptr<uint8_t>(img, const_cast<uint8_t*>(img_plane));
    }

    MemoryStream stream(fileData.constData(), size);
    HeifReader   heif;
    heif.load(stream);
    SefdBox sf = heif.getSefdBox();
    if (sf.getSize() != 0 && sf.getFtyp().getSize() != 0 && sf.getFtyp().GetMajorBand() == "mp42" && sf.getMdat().getSize() != 0 && sf.getMdat().startPosition() != 0 && sf.getMdat().endPosition() != 0 && sf.getFtypStartPos() < sf.getSize())   // guard the length below against underflow
    {
        m_imageEXIF.MicroVideo.HasMicroVideo = 1;
        const size_t start  = heif.getSefdOffset() + sf.getFtypStartPos();
        const size_t length = sf.getSize() - sf.getFtypStartPos();
        if (start <= size && length <= size - start)
        {
            m_memoryVideo.reset(new QBuffer());
            m_memoryVideo->setData(fileData.mid(qsizetype(start), qsizetype(length)));
        }
    }

    // Newer HEIF motion photos keep the clip in a top-level 'mpvd' box (handled by
    // HeifReader above) instead of the Samsung 'sefd' box; hand its MP4 payload over.
    const size_t mpvdStart  = heif.getMpvdVideoOffset();
    const size_t mpvdLength = heif.getMpvdVideoSize();
    if (!m_memoryVideo && mpvdLength != 0 && mpvdStart <= size && mpvdLength <= size - mpvdStart)
    {
        m_imageEXIF.MicroVideo.HasMicroVideo = 1;
        m_memoryVideo.reset(new QBuffer());
        m_memoryVideo->setData(fileData.mid(qsizetype(mpvdStart), qsizetype(mpvdLength)));
    }
}

size_t HeifManager::getWidth() const
{
    return m_width;
}

size_t HeifManager::getHeight() const
{
    return m_height;
}

std::shared_ptr<uint8_t> HeifManager::getDecodedImage()
{
    return m_decodedImage;
}

QImage HeifManager::image() const
{
    if (!m_decodedImage)
    {
        return {};
    }
    auto* keep = new std::shared_ptr<uint8_t>(m_decodedImage);
    return QImage(static_cast<const uchar*>(keep->get()), int(m_width), int(m_height), int(m_stride), QImage::Format_RGBA8888,
                  [](void* p) { delete static_cast<std::shared_ptr<uint8_t>*>(p); }, keep);
}
