#pragma once

#include "TokenValidatorConfig.hpp"
#include "TokenValidationResult.hpp"

class TokenValidator
{
	public:
		TokenValidator(
			const TokenValidatorConfig 	&config,
			JwksProvider				&jwksProvider
		);
		TokenValidationResult	validate(
			const std::string &token
		);
	private:
		TokenValidatorConfig	_config;
		JwksProvider			&_jwksProvider;
};