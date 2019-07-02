#include "tinyexif/TinyEXIF.h"

#include <iostream> // std::cout
#include <fstream>  // std::ifstream
#include <vector>   // std::vector

int main(int argc, const char** argv) {


	// read entire image file
	std::ifstream file("D:\\_pictures\\DCIMS10\\2019-07-01\\20190630_181236.jpg", std::ifstream::in | std::ifstream::binary);
	file.seekg(0, std::ios::end);
	std::streampos length = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(length);
	file.read((char*)data.data(), length);

	// parse image EXIF and XMP metadata
	TinyEXIF::EXIFInfo imageEXIF(data.data(), length);
	if (imageEXIF.Fields)
		std::cout
		<< "Image Description " << imageEXIF.ImageDescription << "\n"
		<< "Image Resolution " << imageEXIF.ImageWidth << "x" << imageEXIF.ImageHeight << " pixels\n"
		<< "Camera Model " << imageEXIF.Make << " - " << imageEXIF.Model << "\n"
		<< "Focal Length " << imageEXIF.FocalLength << " mm" << std::endl;

	TinyEXIF::EXIFInfo imageXMP(data.data(), length);
	imageXMP.parseFromXMPSegment(data.data(), length);

	return 0;
}