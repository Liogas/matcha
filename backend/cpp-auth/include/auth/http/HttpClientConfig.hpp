#pragma once

#include <chrono>
#include <optional>
#include <cstddef>
#include <string>

namespace	auth
{
	struct HttpClientConfig
	{
		std::optional<std::string>	caCertPath;
		std::chrono::milliseconds	connectionTimeout{2000};
		std::chrono::milliseconds	readTimeout{5000};
		std::chrono::milliseconds	writeTimeout{5000};
		std::size_t					maxResponseSize{64 * 1024};
	};
}