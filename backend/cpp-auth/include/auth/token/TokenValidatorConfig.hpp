#pragma once

#include <chrono>
#include <string>
#include <auth/token/TokenPolicy.hpp>

namespace auth
{
	struct	TokenValidatorConfig
	{
		std::string issuer;
		std::string audience;
		std::string algorithm;
		std::chrono::seconds maxLifetime{TOKEN_MAX_LIFETIME};
		std::chrono::seconds clockSkew{60};
	};
}