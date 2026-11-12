#include "heifpreview.h"

#include <QFile>

#include <memory>
#include <type_traits>

#include <libheif/heif.h>

#include "heifdecode.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

namespace
{
    using HeifContextPtr = std::unique_ptr<heif_context, decltype(&heif_context_free)>;
    using HeifHandlePtr  = std::unique_ptr<heif_image_handle, decltype(&heif_image_handle_release)>;

    HeifContextPtr checked(HeifContextPtr ctx, heif_error err)
    {
        if (err.code != heif_error_Ok)
        {
            ctx.reset();
        }
        return ctx;
    }

    HeifContextPtr openContext(const QString& path)
    {
        HeifContextPtr ctx(heif_context_alloc(), &heif_context_free);
        const heif_error err = heif_context_read_from_file(ctx.get(), QFile::encodeName(path).constData(), nullptr);
        return checked(std::move(ctx), err);
    }

    // Reads straight from fileData, which must outlive the returned context.
    HeifContextPtr openContext(const QByteArray& fileData)
    {
        HeifContextPtr   ctx(heif_context_alloc(), &heif_context_free);
        const heif_error err = heif_context_read_from_memory_without_copy(ctx.get(), fileData.constData(),
                                                                           size_t(fileData.size()), nullptr);
        return checked(std::move(ctx), err);
    }

    HeifHandlePtr primaryHandle(heif_context* ctx)
    {
        heif_image_handle* handle = nullptr;
        heif_context_get_primary_image_handle(ctx, &handle);
        return HeifHandlePtr(handle, &heif_image_handle_release);
    }

    // The QImage takes ownership of the decoded heif_image, so the pixels are not copied.
    QImage decode(heif_image_handle* handle)
    {
        heif_image* img = nullptr;
        if (heifDecodeRGBA(handle, &img).code != heif_error_Ok)
        {
            return {};
        }
        int            stride = 0;
        const uint8_t* plane  = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
        if (!plane)
        {
            heif_image_release(img);
            return {};
        }
        return QImage(plane, heif_image_get_width(img, heif_channel_interleaved),
                      heif_image_get_height(img, heif_channel_interleaved), stride, QImage::Format_RGBA8888,
                      [](void* p) { heif_image_release(static_cast<heif_image*>(p)); }, img);
    }

    QImage libheifThumbnail(heif_context* ctx, bool allowFullDecode)
    {
        if (!ctx)
        {
            return {};
        }
        HeifHandlePtr primary = primaryHandle(ctx);
        if (!primary)
        {
            return {};
        }

        heif_item_id       id    = 0;
        heif_image_handle* thumb = nullptr;
        if (heif_image_handle_get_list_of_thumbnail_IDs(primary.get(), &id, 1) == 1)
        {
            heif_image_handle_get_thumbnail(primary.get(), id, &thumb);
        }
        HeifHandlePtr thumbGuard(thumb, &heif_image_handle_release);
        if (!thumb && !allowFullDecode)
        {
            return {};
        }
        return decode(thumb ? thumb : primary.get());
    }

    QImage fitBox(QImage img, int box)
    {
        if (img.width() > box || img.height() > box)
        {
            img = img.scaled(box, box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return img;
    }

#ifdef __APPLE__
    struct CFReleaser
    {
        void operator()(CFTypeRef ref) const
        {
            CFRelease(ref);
        }
    };
    template <typename T>
    using CFPtr = std::unique_ptr<std::remove_pointer_t<T>, CFReleaser>;

    // Drawing into an 8-bit sRGB buffer also converts from the photo's colour profile (Display P3 on iPhones).
    QImage toQImage(CGImageRef cg)
    {
        const size_t w = CGImageGetWidth(cg);
        const size_t h = CGImageGetHeight(cg);
        QImage       out(int(w), int(h), QImage::Format_RGBA8888_Premultiplied);
        if (out.isNull())
        {
            return {};
        }
        CFPtr<CGColorSpaceRef> srgb(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        CFPtr<CGContextRef>    ctx(CGBitmapContextCreate(out.bits(), w, h, 8, size_t(out.bytesPerLine()), srgb.get(),
                                                         uint32_t(kCGImageAlphaPremultipliedLast) | uint32_t(kCGBitmapByteOrder32Big)));
        if (!ctx)
        {
            return {};
        }
        CGContextDrawImage(ctx.get(), CGRectMake(0, 0, CGFloat(w), CGFloat(h)), cg);
        return out;
    }

    CFPtr<CGImageSourceRef> imageSource(const QString& path)
    {
        const QByteArray file = QFile::encodeName(path);
        CFPtr<CFURLRef>  url(CFURLCreateFromFileSystemRepresentation(
            nullptr, reinterpret_cast<const UInt8*>(file.constData()), file.size(), false));
        return CFPtr<CGImageSourceRef>(url ? CGImageSourceCreateWithURL(url.get(), nullptr) : nullptr);
    }

    // Wraps fileData without copying it; it must outlive the returned source.
    CFPtr<CGImageSourceRef> imageSource(const QByteArray& fileData)
    {
        CFPtr<CFDataRef> data(CFDataCreateWithBytesNoCopy(nullptr, reinterpret_cast<const UInt8*>(fileData.constData()),
                                                          fileData.size(), kCFAllocatorNull));
        return CFPtr<CGImageSourceRef>(data ? CGImageSourceCreateWithData(data.get(), nullptr) : nullptr);
    }

    // Embedded thumbnail only: ImageIO reads it several times faster than libde265 decodes it,
    // but its full-image decode is slower than libheif's, so files without one go to libheif.
    QImage imageioEmbeddedThumbnail(CGImageSourceRef src, int box)
    {
        if (!src)
        {
            return {};
        }
        int                    maxPx = box;
        CFPtr<CFNumberRef>     maxNum(CFNumberCreate(nullptr, kCFNumberIntType, &maxPx));
        const void*            keys[] = { kCGImageSourceCreateThumbnailFromImageIfAbsent,
                                          kCGImageSourceCreateThumbnailWithTransform, kCGImageSourceThumbnailMaxPixelSize };
        const void*            vals[] = { kCFBooleanFalse, kCFBooleanTrue, maxNum.get() };
        CFPtr<CFDictionaryRef> opts(CFDictionaryCreate(nullptr, keys, vals, 3, &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks));
        CGImageRef             cg = CGImageSourceCreateThumbnailAtIndex(src, 0, opts.get());
        if (!cg)
        {
            return {};
        }
        CFPtr<CGImageRef> cgGuard(cg);
        return toQImage(cg);
    }
#endif
}   // namespace

QImage HeifPreview::thumbnail(const QString& path, int box, bool allowFullDecode)
{
    QImage img;
#ifdef __APPLE__
    img = imageioEmbeddedThumbnail(imageSource(path).get(), box);
#endif
    if (img.isNull())
    {
        img = libheifThumbnail(openContext(path).get(), allowFullDecode);
    }
    return fitBox(img, box);
}

QImage HeifPreview::thumbnail(const QByteArray& fileData, int box, bool allowFullDecode)
{
    QImage img;
#ifdef __APPLE__
    img = imageioEmbeddedThumbnail(imageSource(fileData).get(), box);
#endif
    if (img.isNull())
    {
        img = libheifThumbnail(openContext(fileData).get(), allowFullDecode);
    }
    return fitBox(img, box);
}

QSize HeifPreview::size(const QByteArray& fileData)
{
    HeifContextPtr ctx = openContext(fileData);
    if (!ctx)
    {
        return {};
    }
    HeifHandlePtr primary = primaryHandle(ctx.get());
    if (!primary)
    {
        return {};
    }
    return QSize(heif_image_handle_get_width(primary.get()), heif_image_handle_get_height(primary.get()));
}
