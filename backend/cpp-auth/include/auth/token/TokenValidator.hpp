#pragma once

#include <auth/token/TokenValidatorInterface.hpp>
#include <auth/jwks/JwksProvider.hpp>
#include <auth/token/TokenValidatorConfig.hpp>
#include <jwt-cpp/jwt.h>

namespace auth
{
	class TokenValidator : public TokenValidatorInterface
	{
		public:
			TokenValidator(
				const TokenValidatorConfig 	&config,
				JwksProvider				&jwksProvider
			);
			TokenValidationResult	validate(
				const std::string &token
			) override;
		private:
			TokenValidatorConfig	_config;
			JwksProvider			&_jwksProvider;

			bool	validateAlgorithm(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
			);
			bool	validateIssuer(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
			);
			bool	validateAudience(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
			);
			bool	validateExpiration(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
			);
			JwksResult	getJwk(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
			);
			bool	verifSignature(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded,
				const Jwk &jwk
			);
			TokenValidationResult	invalidToken();
			TokenValidationResult	validToken(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
			);
			TokenValidationResult	verifUnavailable();

			AuthenticatedIdentity	extractClaims(
				const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
			);
			
	};
}