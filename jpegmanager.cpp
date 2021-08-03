#include "jpegmanager.h"

#include <Tools/jpeginmemoryvideo.h>

#include <QDebug>
#include <QImage>

#include <iostream>
#include <fstream>


JpegManager::JpegManager(const QString& imgPath)
    : m_imagePath(imgPath)
    , m_memoryVideo(nullptr)
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
            m_memoryVideo.reset(new JpegUtils::InMemoryVid(data.data() + offset, data.size() - offset));
        }
    }
}

bool JpegManager::HasMicrovideo()
{
    return m_imageEXIF.MicroVideo.HasMicroVideo;
}

std::shared_ptr<JpegUtils::InMemoryVid> JpegManager::getVideoMemory()
{
    return m_memoryVideo;
}
