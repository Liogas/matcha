#pragma once

#include <optional>
#include "Jwk.hpp"

namespace	auth
{
	struct JwksCacheResult
	{
		enum class Status
		{
			Found,
			NotFound,
			Expired
		};
		Status status;
		std::optional<Jwk> key;
	};
}