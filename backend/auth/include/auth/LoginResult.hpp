#pragma once

#include "User.hpp"
#include <optional>

namespace auth
{
	struct	LoginResult
	{
		enum class Status
		{
			Success,
			InvalidCredentials,
			InternalError
		};
		Status status;
		std::optional<User> user;
	};
}