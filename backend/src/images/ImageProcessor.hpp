#pragma once

#include "ImageTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ImageProcessor
{
	public:
		ImageProcessor(
			std::size_t maximumInputSize,
			std::uint32_t maximumDimension,
			int webpQuality
		);
		~ImageProcessor();

		ImageProcessor(const ImageProcessor &) = delete;
		ImageProcessor &operator=(const ImageProcessor &) = delete;

		ProcessedImage process(
			const std::vector<std::uint8_t> &input,
			const std::string &contentType
		) const;

	private:
		std::size_t _maximumInputSize;
		std::uint32_t _maximumDimension;
		int _webpQuality;
};
