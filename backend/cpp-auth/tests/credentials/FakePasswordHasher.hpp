#pragma once

#include <auth/credentials/IPasswordHasher.hpp>
#include <stdexcept>

class FakePasswordHasher : public auth::IPasswordHasher
{
	public:
		std::string lastPwd;
		bool		failHash = false;
		bool		failVerify = false;
		std::string	hash(const std::string &pwd) override
		{
			if (failHash)
				throw std::runtime_error("fake hashing failure");
			lastPwd = pwd;
			return "hashed_" + pwd;
		}
		bool	verify(
			const std::string &password,
			const std::string &passwordHash
		)
		{
			if (failVerify)
				throw std::runtime_error("fake verification failure");
			return passwordHash == "hashed_" + password;
		}
};