#pragma once

#include <auth/token/TokenValidationResult.hpp>

namespace auth
{
	class TokenValidatorInterface
	{
		public:
			virtual ~TokenValidatorInterface() = default;
			virtual TokenValidationResult	validate(
				const std::string &token
			) = 0;
	};
}