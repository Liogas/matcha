#pragma once

#include <cstdint>
#include <string>

namespace auth
{
	struct AuthenticatedUser
	{
		std::int64_t	id;
		std::string		email;
	};
}