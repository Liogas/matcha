#pragma once

#include <chrono>
#include <string>

namespace auth
{
	struct SessionRecord
	{
		std::string tokenHash;
		std::string subject;
		std::chrono::system_clock::time_point expiresAt;
	};
}
