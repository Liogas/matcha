#pragma once

#include "AuthenticatedUser.hpp"
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
		std::optional<AuthenticatedUser> user;
	};
}