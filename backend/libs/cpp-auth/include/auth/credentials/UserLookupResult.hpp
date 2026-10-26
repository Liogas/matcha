#pragma once

#include "User.hpp"
#include <optional>

namespace auth
{
	struct UserLookupResult
	{
		enum class Status
		{
			Found,
			NotFound,
			DatabaseError
		};
		Status status;
		std::optional<User> user;
	};
}