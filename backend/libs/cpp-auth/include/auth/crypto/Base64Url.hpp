#pragma once

#include <jwt-cpp/base.h>

#include <string>

namespace auth
{
	inline std::string encodeBase64Url(const std::string &data)
	{
		std::string encoded =
			jwt::base::encode<jwt::alphabet::base64url>(data);
		constexpr char encodedPadding[] = "%3d";
		constexpr std::size_t paddingSize = sizeof(encodedPadding) - 1;

		while (encoded.size() >= paddingSize &&
			encoded.compare(
				encoded.size() - paddingSize,
				paddingSize,
				encodedPadding
			) == 0)
			encoded.resize(encoded.size() - paddingSize);

		return encoded;
	}
}
