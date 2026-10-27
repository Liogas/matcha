#include <core/Error.hpp>

std::string_view	core::toString(ErrorCode code) noexcept
{
	switch (code)
	{
		case ErrorCode::Invalid:		return "invalid";
		case ErrorCode::NotFound:		return "not_found";
		case ErrorCode::Conflict:		return "conflict";
		case ErrorCode::Unauthorized:	return "unauthorized";
		case ErrorCode::Forbidden:		return "forbidden";
		case ErrorCode::Unavailable:	return "unavailable";
		case ErrorCode::Internal:		return "internal";
	}
	return "internal";
}