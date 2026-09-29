#pragma once

#include <chrono>
#include <string>

namespace	auth
{
	struct HttpJwksConfig
	{
		std::string jwksUrl;
		std::chrono::seconds cacheTtl;
	};
}