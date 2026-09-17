#pragma once

#include "User.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace auth
{
	class IUserRepository
	{
		public:
			virtual	~IUserRepository() = default;
			virtual std::optional<User>
				findByEmail(const std::string &email) = 0;
			virtual std::optional<User>
				findById(std::int64_t id) = 0;
			virtual	std::int64_t
				createUser(
					const std::string &email,
					const std::string &passwordHash
				) = 0;
	};
}