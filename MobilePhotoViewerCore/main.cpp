#include "tinyexif/TinyEXIF.h"

#include <iostream> // std::cout
#include <fstream>  // std::ifstream
#include <vector>   // std::vector
#include <regex>

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

int main(int argc, const char** argv) {


	// read entire image file
	std::ifstream file("D:\\_pictures\\DCIMS10\\2019-07-01\\20190630_181236.jpg", std::ifstream::in | std::ifstream::binary);
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
	}

	TinyEXIF::EXIFInfo imageXMP(data.data(), length);
	imageXMP.parseFromXMPSegment(data.data(), length);

	return 0;
}