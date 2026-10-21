#include "ImageProcessor.hpp"

#include <openssl/sha.h>
#include <stdexcept>
#include <vips/vips8>

using vips::VImage;

namespace
{
	std::string sha256Hex(const std::vector<std::uint8_t> &content)
	{
		unsigned char digest[SHA256_DIGEST_LENGTH];
		SHA256(content.data(), content.size(), digest);
		static constexpr char HEX[] = "0123456789abcdef";
		std::string result;
		result.reserve(SHA256_DIGEST_LENGTH * 2);
		for (const auto byte : digest)
		{
			result += HEX[byte >> 4];
			result += HEX[byte & 0x0f];
		}
		return result;
	}

	bool supportedContentType(const std::string &contentType)
	{
		return contentType == "image/jpeg" ||
			contentType == "image/png" ||
			contentType == "image/webp";
	}
}

ImageProcessor::ImageProcessor(
	std::size_t maximumInputSize,
	std::uint32_t maximumDimension,
	int webpQuality
):
	_maximumInputSize(maximumInputSize),
	_maximumDimension(maximumDimension),
	_webpQuality(webpQuality)
{
	if (vips_init("matcha") != 0)
		throw std::runtime_error("Unable to initialize libvips");
}

ImageProcessor::~ImageProcessor()
{
	vips_shutdown();
}

ProcessedImage ImageProcessor::process(
	const std::vector<std::uint8_t> &input,
	const std::string &contentType
) const
{
	if (input.empty() || input.size() > _maximumInputSize)
		throw std::invalid_argument("Image exceeds the allowed size");
	if (!supportedContentType(contentType))
		throw std::invalid_argument("Unsupported image content type");

	VImage image;
	try
	{
		image = VImage::new_from_buffer(
			input.data(),
			input.size(),
			"",
			VImage::option()->set("access", "sequential")
		).autorot();
		if (image.width() <= 0 || image.height() <= 0)
			throw std::invalid_argument("Image has invalid dimensions");
	}
	catch (const std::invalid_argument &)
	{
		throw;
	}
	catch (const std::exception &error)
	{
		throw std::invalid_argument(
			std::string("Invalid image: ") + error.what()
		);
	}

	const auto largestDimension = static_cast<double>(
		std::max(image.width(), image.height())
	);
	if (largestDimension > _maximumDimension)
	{
		const auto scale =
			static_cast<double>(_maximumDimension) / largestDimension;
		image = image.resize(scale, VImage::option()->set("kernel", "lanczos3"));
	}

	void *encoded = nullptr;
	std::size_t encodedSize = 0;
	try
	{
		image.write_to_buffer(
			".webp",
			&encoded,
			&encodedSize,
			VImage::option()->set("Q", _webpQuality)
		);
	}
	catch (const std::exception &error)
	{
		throw std::invalid_argument(
			std::string("Unable to process image: ") + error.what()
		);
	}
	if (encoded == nullptr || encodedSize == 0)
		throw std::runtime_error("Unable to encode image");

	std::vector<std::uint8_t> output(
		static_cast<std::uint8_t *>(encoded),
		static_cast<std::uint8_t *>(encoded) + encodedSize
	);
	g_free(encoded);
	return {
		{
			"image/webp",
			output.size(),
			static_cast<std::uint32_t>(image.width()),
			static_cast<std::uint32_t>(image.height()),
			sha256Hex(output)
		},
		std::move(output)
	};
}
