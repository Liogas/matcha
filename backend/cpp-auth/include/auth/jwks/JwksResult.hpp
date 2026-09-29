#pragma once

#include "Jwk.hpp"
#include <optional>

struct JwksResult
{
	enum class Status
	{
		Success,
		KeyNotFound,
		Unavailable
	};
	Status 						status;
	std::optional<auth::Jwk>	key;
};