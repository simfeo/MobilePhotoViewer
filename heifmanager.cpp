#include "heifmanager.h"
#include <Tools/jpeginmemoryvideo.h>

#include <QDebug>
#include <QImage>

#include <iostream>
#include <fstream>  // std::ifstream
#include <videodecoder.h>

//#include <heifreader.h>
//#include <Heif.h>

#include <heif.h>
#include <heif_context.h>
#include <heif_api_structs.h>

#include <Tools/heifreader.h>

HeifManager::HeifManager(const QString& imgPath)
    : m_width(0)
    , m_height(0)
    , m_imagePath(imgPath)
    , m_decodedImage(nullptr)
    , m_memoryVideo(nullptr)
{
    manageHeif();
}

void HeifManager::manageHeif()
{
    std::shared_ptr<std::ifstream> istr(new std::ifstream(m_imagePath.toStdString().c_str(), std::ios_base::binary), [](std::ifstream* p) {if (p) { p->close(), delete p; }});
    uint8_t magic[12];
    istr->read((char*)magic, 12);
    heif_filetype_result filetype_check = heif_check_filetype(magic, 12);
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

    std::shared_ptr<heif_context> ctx(heif_context_alloc(), [](heif_context* p) {heif_context_free(p); });
    if (!ctx)
    {
        qDebug() << "Could not create context object\n";
        return;
    }

    struct heif_error err;
    err = heif_context_read_from_file(ctx.get(), m_imagePath.toStdString().c_str(), nullptr);
    if (err.code != 0)
    {
        qDebug() << "Could not read HEIF/AVIF file: " << err.message;
        return;
    }

    int num_images = heif_context_get_number_of_top_level_images(ctx.get());
    if (num_images == 0)
    {
        qDebug() << "File doesn't contain any images\n";
        return;
    }

    qDebug() << "File contains " << num_images << " images\n";

    std::vector<heif_item_id> image_IDs(num_images);
    num_images = heif_context_get_list_of_top_level_image_IDs(ctx.get(), image_IDs.data(), num_images);

    for (int idx = 0; idx < num_images; ++idx)
    {
        struct heif_image_handle* handle;
        err = heif_context_get_image_handle(ctx.get(), image_IDs[idx], &handle);
        if (err.code)
        {
            qDebug() << "Could not read HEIF/AVIF image " << idx << ": " << err.message;
            return;
        }

//        int has_alpha = heif_image_handle_has_alpha_channel(handle);
        std::shared_ptr<heif_decoding_options> decode_options(heif_decoding_options_alloc(), [](heif_decoding_options* p) {heif_decoding_options_free(p); });
        //      encoder->UpdateDecodingOptions(handle, decode_options);

        int bit_depth = heif_image_handle_get_luma_bits_per_pixel(handle);
        if (bit_depth < 0)
        {
            heif_decoding_options_free(decode_options.get());
            heif_image_handle_release(handle);
            qDebug() << "Input image has undefined bit-depth\n";
            return;
        }

        struct heif_image* image;
        //if (has_alpha)
        //{
        err = heif_decode_image(handle,
            &image,
            heif_colorspace_RGB,
            heif_chroma_interleaved_RGBA,
            decode_options.get());
        //}
        //else
        //{
        //    err = heif_decode_image(handle,
        //        &image,
        //        heif_colorspace_RGB,
        //        heif_chroma_interleaved_RGB,
        //        decode_options.get());
        //}
        if (err.code)
        {
            heif_image_handle_release(handle);
            qDebug() << "Could not decode image: " << idx << ": " << err.message;
            return;
        }

        std::shared_ptr<heif_image> img(image, [](heif_image* p) { heif_image_release(p); });

        qDebug() << "decoded image dimesions" << img->image->get_width() << "x" << img->image->get_height();

        int r_stride = 0;
        const uint8_t* img_plane = heif_image_get_plane_readonly(img.get(), heif_channel_interleaved, &r_stride);

        m_width = heif_image_get_width(img.get(), heif_channel_interleaved);
        m_height = heif_image_get_height(img.get(), heif_channel_interleaved);
        qDebug() << "native dimesions" << m_width << "x" << m_height;
        if (img_plane)
        {
            m_decodedImage.reset(new uint8_t[m_height * r_stride], [](uint8_t* p) { delete[] p; });
            memcpy(m_decodedImage.get(), img_plane, m_height * r_stride);
        }

        for (const auto& metadata : handle->image->get_metadata())
        {
            if (metadata->item_type == "exif"
                || metadata->item_type == "Exif")
            {
                m_imageEXIF = TinyEXIF::EXIFInfo();
                m_imageEXIF.clear();
                m_imageEXIF.parseFromEXIFSegment(&metadata->m_data[4], metadata->m_data.size() - 4);
            }
        }
    }


    
    HeifReader heif;
    heif.load(*istr.get());
    SefdBox sf = heif.getSefdBox();
    if (sf.getSize() != 0
        && sf.getFtyp().getSize() != 0
        && sf.getFtyp().GetMajorBand() == "mp42"
        && sf.getMdat().getSize() != 0
        && sf.getMdat().startPosition() != 0
        && sf.getMdat().endPosition() != 0)
    {
        m_imageEXIF.MicroVideo.HasMicroVideo = 1;
        std::vector<uint8_t> videoData;
        videoData.resize(sf.getSize() - sf.getFtypStartPos());
        if (istr->good())
        {
            istr->seekg(heif.getSefdOffset() + sf.getFtypStartPos());
            istr->read(reinterpret_cast<char*>(&videoData[0]), videoData.size());
            m_memoryVideo.reset(new JpegUtils::InMemoryVid(videoData));
        }
        else
        {
            istr.reset(new std::ifstream(m_imagePath.toStdString().c_str(), std::ios_base::binary), [](std::ifstream* p) {if (p) { p->close(), delete p; }});

            istr->seekg(heif.getSefdOffset() + sf.getFtypStartPos());
            istr->read(reinterpret_cast<char*>(&videoData[0]), videoData.size());
            m_memoryVideo.reset(new JpegUtils::InMemoryVid(videoData));
        }
    }
    /*
    if (heif.hasSingleImage())
    {
        qDebug() << "has single image";
    }
    
    if (heif.hasImageCollection())
    {
        qDebug() << "image collection";
    }
    
    
    std::shared_ptr<HEIF::Reader> reader(HEIF::Reader::Create(), [](HEIF::Reader* p) {HEIF::Reader::Destroy(p); });

    if (reader->initialize(m_imagePath.toStdString().c_str()) != HEIF::ErrorCode::OK)
    {
        return;
    }

    HEIF::FileInformation fileInfo{};
    reader->getFileInformation(fileInfo);


    // Verify that the file has one or several images in the MetaBox
    if (!(fileInfo.features & HEIF::FileFeatureEnum::HasSingleImage || fileInfo.features & HEIF::FileFeatureEnum::HasImageCollection))
    {
        return;
    }

    // Find the primary item ID.
    */
    //int primaryItemId = -1;
    //heif.getPrimaryItem(primaryItemId);

    /*
    const HEIF::FourCC grid("grid");
    HEIF::FourCC primaryType;
    reader->getItemType(primaryItemId, primaryType);
    */
   /*
    reader->getWidth(primaryItemId, m_width);
    reader->getHeight(primaryItemId, m_height);

    HEIF::ErrorCode image_decoded;

    if (primaryType == grid)
    {
        HEIF::Array<HEIF::ImageId> masterIds;
        reader->getMasterImages(masterIds);

    }
    else
    {

        qDebug() << primaryItemId.get() << m_width << "x" << m_height;

        size_t imgBufSize = m_width * m_height * 4;
        m_decodedImage.reset(new uint8_t[imgBufSize], [](uint8_t* p) {delete[] p; });

        image_decoded = reader->getItemDataWithDecoderParameters(primaryItemId, m_decodedImage.get(), imgBufSize);
    }
    */
    // Find item(s) referencing to the primary item with "cdsc" (content describes) item reference.
   
    /*
    HEIF::Array<HEIF::ImageId> metadataIds;
    reader->getReferencedToItemListByType(primaryItemId, "cdsc", metadataIds);
    HEIF::ImageId exifItemId = metadataIds[0];

    // Optional: verify the item ID we got is really of "Exif" type.
    HEIF::FourCC itemType;
    reader->getItemType(exifItemId, itemType);
    if (itemType != "Exif" && !m_decodedImage)
    {
        return;
    }

    // Get item size from parsed information.
    const auto it = std::find_if(fileInfo.rootMetaBoxInformation.itemInformations.begin(),
                                 fileInfo.rootMetaBoxInformation.itemInformations.end(),
                                 [exifItemId](HEIF::ItemInformation a) { return a.itemId == exifItemId; });
    if (it == fileInfo.rootMetaBoxInformation.itemInformations.end())
    {
        return;
    }
    size_t itemSize = it->size;

    // Request item data.
    std::shared_ptr<uint8_t> memoryBuffer(new uint8_t[itemSize], [](uint8_t* p) {delete[] p; });
    reader->getItemData(metadataIds[0], memoryBuffer.get(), itemSize);


    m_imageEXIF = TinyEXIF::EXIFInfo();
    m_imageEXIF.clear();
    m_imageEXIF.parseFromEXIFSegment(memoryBuffer.get()+4, itemSize-4);
    */
    
    /*
    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Find the item ID of the first master image
    HEIF::Array<HEIF::ImageId> masterIds;
    reader->getMasterImages(masterIds);
    const HEIF::ImageId masterId = masterIds[0];


    itemSize = 1024 * 1024;
    auto* itemData = new uint8_t[itemSize];

    for (const auto masterId : masterIds)
    {
        uint32_t width, height;
        reader->getWidth(masterId, width);
        reader->getHeight(masterId, height);
        qDebug() << masterId.get() << width << "x" << height;
    }

    HEIF::Array<HEIF::ImageId> identityIds;
    reader->getItemListByType("iden", identityIds);

    HEIF::Array<HEIF::ImageId> referencedImages;
    reader->getReferencedFromItemListByType(identityIds[0], "dimg", referencedImages);

    HEIF::Array<HEIF::ItemPropertyInfo> propertyInfos;
    reader->getItemProperties(identityIds[0], propertyInfos);

    unsigned int rotation = 0;
    for (const auto& property : propertyInfos)
    {
        if (property.type == HEIF::ItemPropertyType::IROT)
        {
            HEIF::Rotate irot;
            reader->getProperty(property.index, irot);
            rotation = irot.angle;
        }
    }
    */
}

bool HeifManager::HasMicrovideo()
{
    return m_imageEXIF.MicroVideo.HasMicroVideo;
}

std::shared_ptr<JpegUtils::InMemoryVid> HeifManager::getVideoMemory()
{
    return m_memoryVideo;
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
