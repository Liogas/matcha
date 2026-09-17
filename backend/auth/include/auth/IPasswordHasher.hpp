#pragma once

#include <string>

namespace auth
{
	class IPasswordHasher
	{
		public:
			virtual ~IPasswordHasher() = default;
			virtual std::string hash(const std::string &pwd) = 0;
	};
}