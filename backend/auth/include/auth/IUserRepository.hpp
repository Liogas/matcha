#pragma once

#include "UserLookupResult.hpp"
#include "CreateUserResult.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace auth
{
	class IUserRepository
	{
		public:
			virtual	~IUserRepository() = default;
			virtual UserLookupResult
				findByEmail(const std::string &email) = 0;
			virtual UserLookupResult
				findById(std::int64_t id) = 0;
			virtual	CreateUserResult
				createUser(
					const std::string &email,
					const std::string &passwordHash
				) = 0;
	};
}