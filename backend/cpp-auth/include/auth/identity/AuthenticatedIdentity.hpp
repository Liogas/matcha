#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace auth
{
	struct AuthenticatedIdentity
	{
		std::string subject;
		std::string issuer;
		std::vector<std::string> audience;
	};
}