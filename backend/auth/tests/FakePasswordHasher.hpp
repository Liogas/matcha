#pragma once

#include <auth/IPasswordHasher.hpp>

class FakePasswordHasher : public auth::IPasswordHasher
{
	public:
		std::string lastPwd;
		std::string	hash(const std::string &pwd) override
		{
			lastPwd = pwd;
			return "hashed_" + pwd;
		}
};