#pragma once
#include <core/Error.hpp>
#include <expected>
#include <string>
#include <utility>

namespace core
{
	// Une valeur T, ou une Error expliquant pourquoi il n'y en a pas.
	template <class T>
	using Result = std::expected<T, Error>;

	// return core::fail(core::ErrorCode::NotFound, "account not found");
	inline std::unexpected<Error> fail(ErrorCode code, std::string message)
	{
		return std::unexpected<Error>(Error{code, std::move(message)});
	}
}