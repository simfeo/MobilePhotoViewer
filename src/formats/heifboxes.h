#ifndef HEIFBOXES_H
#define HEIFBOXES_H
#include <cstdint>   // std::int64_t / std::uint64_t (<stdint.h> only declares them globally)
#include <string>
#include <vector>
#include <memory>

namespace Utils
{
    class RamFile;
}


class HeifBoxBase
{
public:
    HeifBoxBase(std::shared_ptr<Utils::RamFile> boxMem);
    void parseHeaders();
    const std::string getType() const;
    const uint64_t getSize() const;
protected:
    HeifBoxBase();
    uint64_t                    m_size;
    std::string                 m_type;
    std::vector<uint8_t>        m_uid;
    std::shared_ptr<Utils::RamFile> m_boxMemory;
};

class FtypBox : public HeifBoxBase
{
public:
    FtypBox();
    FtypBox(std::shared_ptr<Utils::RamFile> boxMem);
    const std::string GetMajorBand() const;
    const std::vector<std::string> getMinorBrands() const;
private:
    void parseHeaderFull();

    std::uint32_t               m_version;
    std::string                 m_majorBrand;
    std::vector<std::string>    m_minorBrands;
};

class MdatBox : public HeifBoxBase
{
public:
    MdatBox();
    MdatBox(std::shared_ptr<Utils::RamFile> boxMem);
    const uint64_t startPosition() const;
    const uint64_t endPosition() const;
private:
    void parseHeaderFull();

    std::uint64_t               m_startPos;
    std::uint64_t               m_endPos;
};

class SefdBox : public HeifBoxBase
{
public:
    SefdBox();
    SefdBox(std::shared_ptr<Utils::RamFile> boxMem);
    FtypBox& getFtyp();
    MdatBox& getMdat();
    uint64_t getFtypStartPos();

private:
    void parseHeaderFull();
    void parseSefd();
    void fillPropertyByType(std::string& ptype);
    void fillPropertyData(std::string& pdata);

    bool        m_motionPhotoDataFound = false;
    uint8_t     m_version = 0;
    uint32_t    m_flags = 0; // only 24 bit can be set
    uint64_t    m_ftypStartPos;

    std::string m_imageUtcData;
    std::string m_imageMCCData;
    FtypBox     m_ftyp;
    MdatBox     m_mdat;
};
#endif // HEIFBOXES_H
