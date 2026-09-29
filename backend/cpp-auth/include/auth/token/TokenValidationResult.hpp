#pragma once

#include <optional>
#include <auth/identity/AuthenticatedIdentity.hpp>

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