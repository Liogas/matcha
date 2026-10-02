#pragma once

#include <string>

namespace auth
{
	struct	TokenValidatorConfig
	{
		std::string issuer;
		std::string audience;
		std::string algorithm;
	};
}