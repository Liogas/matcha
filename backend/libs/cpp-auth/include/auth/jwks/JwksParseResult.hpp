#pragma once

#include <vector>
#include "Jwk.hpp"

namespace	auth
{
	struct JwksParseResult
	{
		enum class Status
		{
			Success,
			InvalidJwks
		};
		Status status;
		std::vector<Jwk> keys;
	};
}