#pragma once

#include <chrono>
#include <string>

namespace auth
{
	struct TokenGeneratorConfig
	{
		std::string 			issuer;
		std::string 			audience;
		std::chrono::seconds	lifetime;
	};
}