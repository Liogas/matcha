#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ImageMetadata
{
	std::string contentType;
	std::size_t fileSize;
	std::uint32_t width;
	std::uint32_t height;
	std::string sha256;
};

struct ProcessedImage
{
	ImageMetadata metadata;
	std::vector<std::uint8_t> content;
};
