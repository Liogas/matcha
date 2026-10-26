#pragma once

#include <optional>

namespace auth
{
	struct CreateUserResult
	{
		enum class Status
		{
			Success,
			EmailAlreadyExists,
			DatabaseError
		};
		Status status;
		std::optional<std::int64_t> userId;
	};
}