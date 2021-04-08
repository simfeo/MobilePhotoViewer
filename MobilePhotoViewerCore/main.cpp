#include "tinyexif/TinyEXIF.h"

#include <iostream> // std::cout
#include <fstream>  // std::ifstream
#include <vector>   // std::vector
#include <regex>

extern "C"
{
	#include <libavformat/avformat.h>
	#include <libavutil/dict.h>
}

//extern "C" {
//#include "libavcodec/avcodec.h"
//#include "libavdevice/avdevice.h"
//#include "libavfilter/avfilter.h"
//#include "libavformat/avformat.h"
//#include "libavutil/avutil.h"
//}

/*
size_t findMagic(unsigned char *buf, size_t size) {
	const unsigned char magic[] = {
		0x4D, 0x6F, 0x74, 0x69, 0x6F, 0x6E, 0x50, 0x68,
		0x6F, 0x74, 0x6F, 0x5F, 0x44, 0x61, 0x74, 0x61 };
	static char d1[256];
	static char d2[sizeof(magic)];
	bool inited = false;
	size_t i, j;

	if (!inited) {
		initSearch(magic, sizeof(magic), d1, d2);
		inited = TRUE;
	}

	i = sizeof(magic) - 1;
	while (i < size) {
		j = sizeof(magic) - 1;
		while (j >= 0 && (buf[i] == magic[j])) {
			--i; --j;
		}
		if (j < 0)
			return i + 1;
		i += std::max(d1[buf[i]], d2[j]);
	}
	return -1;
}
*/


class InMemoryVid
{
public:
	InMemoryVid(uint8_t* buf, size_t len)
		: m_buffer(buf)
		, m_length(len)
		, m_currentPos(0)
	{
	}

	int FileStreamRead(uint8_t* buf, int buf_size)
	{
		if (m_currentPos < static_cast<int64_t>(m_length))
		{
			size_t bytesRead = buf_size;
			if (bytesRead > (m_length - m_currentPos))
			{
				bytesRead = m_length - m_currentPos;
			}
			memcpy(buf, m_buffer+ m_currentPos, bytesRead);
			m_currentPos += buf_size;
			return static_cast<int>(bytesRead);
		}
		return -1;
	}

	int setPosition(int64_t newPos)
	{
		if (newPos >= 0 && newPos <= static_cast<int64_t>(m_length))
		{
			m_currentPos = newPos;
			return 0;
		}
		return -1;
	}

	int64_t getPosition()
	{
		return m_currentPos;
	}

	size_t getSize()
	{
		return m_length;
	}

private:
	uint8_t*	m_buffer;
	size_t		m_length;
	int64_t		m_currentPos;
};

int FileStreamRead(void* ptr, uint8_t* buf, int buf_size)
{
	std::cout << "read " << buf_size << std::endl;
	// This is your IStream
	InMemoryVid* stream = reinterpret_cast<InMemoryVid*>(ptr);

	int bytesRead = stream->FileStreamRead(buf, buf_size);

	if (bytesRead == -1)
	{
		return AVERROR_EOF;  // End of file
	}

	return bytesRead;
}

int64_t FileStreamSeek(void* ptr, int64_t offset, int whence)
{
	std::cout << "seek"<<std::endl;
	InMemoryVid* stream = reinterpret_cast<InMemoryVid*>(ptr);

	switch (whence)
	{
		case SEEK_SET:
			return stream->setPosition(offset) != -1 ? stream->getPosition() : -1;
		case SEEK_CUR:
			return stream->setPosition(stream->getPosition() + offset) != -1 ? stream->getPosition() : -1;
		case SEEK_END:
			return stream->setPosition(stream->getSize() + offset) != -1 ? stream->getPosition() : -1;
		case AVSEEK_SIZE: 
			{
				// Special whence for determining filesize without any seek.
				return stream->getSize();
			} 
			break;
	}
	return -1;
}


int main(int argc, const char** argv) {


	// read entire image file
	//std::ifstream file("D:\\_pictures\\DCIMS10\\2019-07-01\\20190630_181236.jpg", std::ifstream::in | std::ifstream::binary);
	std::ifstream file(R"(d:\_pictures\DCIMS10\2021-02-21\20210119_112641.jpg)", std::ifstream::in | std::ifstream::binary);

	file.seekg(0, std::ios::end);
	std::streampos length = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(length);
	file.read((char*)data.data(), length);
	std::string ss = (char*)data.data();

	//std::regex reg(R"(MotionPhoto_Data)", std::regex_constants::ECMAScript);
	//std::smatch smat;
	size_t offset = 0;
	/*
	while (true)
	{
		std::regex_search(ss, smat, reg);

		if (smat.size() == 0)
		{
			offset += ss.size() + 1;
			if (offset >= data.size())
				break;
			if (ss.size() > 40)
			{
				int a = 5;
			}
			ss = (char*)(data.data()+offset);
			continue;
		}
		offset += std::string("MotionPhoto_Data").size();
		for (auto el : smat)
		{
			std::cout << el << std::endl;
		}
		break;
	}
	*/
	
	// parse image EXIF and XMP metadata
	TinyEXIF::EXIFInfo imageEXIF(data.data(), length);
	if (imageEXIF.Fields)
		std::cout
		<< "Image Description " << imageEXIF.ImageDescription << "\n"
		<< "Image Resolution " << imageEXIF.ImageWidth << "x" << imageEXIF.ImageHeight << " pixels\n"
		<< "Camera Model " << imageEXIF.Make << " - " << imageEXIF.Model << "\n"
		<< "Focal Length " << imageEXIF.FocalLength << " mm" << std::endl;

	if (imageEXIF.MicroVideo)
	{
		std::cout << "microvideo offset " << imageEXIF.MicroVideoOffset << std::endl;
		std::ofstream ofile("video.mp4", std::ios::binary);
		offset = data.size() - imageEXIF.MicroVideoOffset;
		ofile.write((char*)(data.data()+ offset), data.size() - offset);
		ofile.close();
		/*
		AVFormatContext* fmt_ctx = NULL;
		AVDictionaryEntry* tag = NULL;
		int ret;

		//av_register_all();
		if ((ret = avformat_open_input(&fmt_ctx, "video.mp4", NULL, NULL)))
		{
		}
		else
		{
			while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
				printf("%s=%s\n", tag->key, tag->value);
			avformat_close_input(&fmt_ctx);
		}*/

		const int FILESTREAMBUFFERSZ = 1024*16;


		// A IStream - you choose where it comes from
		//IStream* fileStreamData;

		// Alloc a buffer for the stream
		unsigned char* fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
		if (fileStreamBuffer == nullptr) {
			// out of memory
		}

		InMemoryVid memVid(data.data() + offset, data.size() - offset);

		// Get a AVContext stream
		AVIOContext* ioContext = avio_alloc_context(
			fileStreamBuffer,    // Buffer
			FILESTREAMBUFFERSZ,  // Buffer size
			0,                   // Buffer is only readable - set to 1 for read/write
			&memVid,      // User (your) specified data
			FileStreamRead,      // Function - Reading Packets (see example)
			0,                   // Function - Write Packets
			FileStreamSeek       // Function - Seek to position in stream (see example)
		);
		if (ioContext == nullptr) {
			// out of memory
		}

		// Allocate a AVContext
		AVFormatContext* formatContext = avformat_alloc_context();

		// Set up the Format Context
		formatContext->pb = ioContext;
		formatContext->flags |= AVFMT_FLAG_CUSTOM_IO; // we set up our own IO

		// Open "file" (open our custom IO)
		// Empty string is where filename would go. Doesn't matter since we aren't reading a file
		// NULL params are format and demuxer settings, respectively
		if (avformat_open_input(&formatContext, "", nullptr, nullptr) < 0) {
			// Error opening file
		}
		else
		{
			AVDictionaryEntry* tag = NULL;
			while ((tag = av_dict_get(formatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
			{
				printf("%s=%s\n", tag->key, tag->value);
			}
			av_dump_format(formatContext, 0, "", 0);
		}

		// Do something with the formatContext

		// Free resources!
		avformat_close_input(&formatContext);
		av_free(ioContext);
	}

	TinyEXIF::EXIFInfo  imageXMP(data.data(), length);
	imageXMP.parseFromXMPSegment(data.data(), length);

	return 0;
}