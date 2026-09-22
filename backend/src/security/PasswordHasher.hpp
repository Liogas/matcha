#pragma once

#include <auth/IPasswordHasher.hpp>

namespace auth
{
	class PasswordHasher : public IPasswordHasher
	{
		public:
			PasswordHasher();
			std::string hash(
				const std::string &password
			) override;
			bool		verify(
				const std::string &password,
				const std::string &passwordHash
			) override;
	};
}