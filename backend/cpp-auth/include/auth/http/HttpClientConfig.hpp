#pragma once

#include <optional>
#include <string>

namespace	auth
{
	struct HttpClientConfig
	{
		std::optional<std::string>	caCertPath;
	};
}