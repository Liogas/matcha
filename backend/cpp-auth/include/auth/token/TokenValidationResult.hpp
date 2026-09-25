#pragma once

#include <optional>
#include "AuthenticatedIdentity.hpp"

struct TokenValidationResult
{
	enum class Status
	{
		Valid,
		InvalidToken,
		VerificationUnavailable
	};
	Status status;
	std::optional<AuthenticatedIdentity> identity;
};