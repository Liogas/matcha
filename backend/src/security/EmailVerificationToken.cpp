#include "EmailVerificationToken.hpp"

#include "OpaqueToken.hpp"

#include <stdexcept>

std::string hashEmailVerificationToken(const std::string &token)
{
	try
	{
		return hashOpaqueToken(token);
	}
	catch (const std::invalid_argument &)
	{
		throw std::invalid_argument(
			"Invalid email verification token format"
		);
	}
}

EmailVerificationToken generateEmailVerificationToken()
{
	const auto token = generateOpaqueToken();
	return {token.value, token.hash};
}
