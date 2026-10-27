#pragma once
#include <string>
#include <string_view>

namespace core
{
	enum class ErrorCode
	{
		Invalid,
		NotFound,
		Conflict,
		Unauthorized,
		Forbidden,
		Unavailable,
		Internal
	};

	struct Error
	{
		ErrorCode	code;
		std::string	message;
	};
	std::string_view	toString(ErrorCode code) noexcept;
}