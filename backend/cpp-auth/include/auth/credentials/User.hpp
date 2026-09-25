#pragma once

#include <cstdint>
#include <string>

namespace auth
{
	struct User
	{
		std::int64_t	id;
		std::string		email;
		std::string		passwordHash;
	};
}