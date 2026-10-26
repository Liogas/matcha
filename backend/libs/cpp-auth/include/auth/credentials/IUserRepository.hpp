#pragma once

#include "IUserLookupRepository.hpp"
#include "CreateUserResult.hpp"
#include <string>

namespace auth
{
	class IUserRepository : public IUserLookupRepository
	{
		public:
			virtual	CreateUserResult
				createUser(
					const std::string &email,
					const std::string &passwordHash
				) = 0;
	};
}