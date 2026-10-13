#pragma once

#include <chrono>
#include <string_view>

namespace auth
{
	inline constexpr std::string_view TOKEN_ALGORITHM = "RS256";
	inline constexpr std::chrono::seconds TOKEN_MAX_LIFETIME{3600};
}
