#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>

namespace auth
{
	inline constexpr std::string_view TOKEN_ALGORITHM = "RS256";
	inline constexpr std::chrono::seconds TOKEN_MAX_LIFETIME{3600};
	inline constexpr std::size_t TOKEN_MAX_SIZE_BYTES = 8 * 1024;
}
