#pragma once

#include <string>

struct EmailVerificationToken
{
	std::string value;
	std::string hash;
};

EmailVerificationToken generateEmailVerificationToken();
std::string hashEmailVerificationToken(const std::string &token);
