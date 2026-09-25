#pragma once

#include "TokenValidatorConfig.hpp"
#include "TokenValidationResult.hpp"

class TokenValidator
{
	public:
		TokenValidator(
			const TokenValidatorConfig &config
		);
		TokenValidationResult	validate(
			const std::string &token
		);
};