#pragma once

#include <string>

namespace auth
{
	class IPasswordHasher
	{
		public:
			virtual ~IPasswordHasher() = default;
			virtual std::string hash(const std::string &pwd) = 0;
			virtual bool		verify(
				const std::string &password,
				const std::string &passwordHash
			) = 0;
	};
}