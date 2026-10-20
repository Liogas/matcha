#pragma once

#include "UserLookupResult.hpp"

#include <cstdint>
#include <string>

namespace auth
{
	class IUserLookupRepository
	{
		public:
			virtual ~IUserLookupRepository() = default;
			virtual UserLookupResult
				findByEmail(const std::string &email) = 0;
			virtual UserLookupResult
				findById(std::int64_t id) = 0;
	};
}
