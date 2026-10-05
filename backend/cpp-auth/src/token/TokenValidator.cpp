#include <auth/token/TokenValidator.hpp>
#include <jwt-cpp/jwt.h>

namespace auth
{
	TokenValidator::TokenValidator(
		const TokenValidatorConfig &config,
		auth::JwksProvider &jwksProvider
	):
		_config(config),
		_jwksProvider(jwksProvider)
	{}

	TokenValidationResult	TokenValidator::validate(
		const std::string &token
	)
	{
		try
		{
			const auto decoded = jwt::decode(token);
			const std::string algo = decoded.get_algorithm();
			if (algo != this->_config.algorithm)
				return {
					TokenValidationResult::Status::InvalidToken,
					std::nullopt
				};
			const std::string issuer =
				decoded.get_payload_claim("iss").as_string();
			if (issuer != this->_config.issuer)
				return {
					TokenValidationResult::Status::InvalidToken,
					std::nullopt
				};
			const std::string kid =
				decoded.get_header_claim("kid").as_string();
			const auto jwksResult =
				this->_jwksProvider.getKey(kid);
			if (jwksResult.status == auth::JwksResult::Status::Unavailable)
			return {
				TokenValidationResult::Status::VerificationUnavailable,
				std::nullopt
			};
			if (jwksResult.status == auth::JwksResult::Status::KeyNotFound)
				return {
					TokenValidationResult::Status::InvalidToken,
					std::nullopt
				};
			if (!jwksResult.key.has_value())
				return {
					TokenValidationResult::Status::InvalidToken,
					std::nullopt
				};
			// VERIFICATION
			const auto &jwk = jwksResult.key.value();
			const auto publicKey = 
				jwt::helper::create_public_key_from_rsa_components(
					jwk.n,
					jwk.e
				);
			const auto algorithm = jwt::algorithm::rs256(publicKey);
			jwt::verify()
				.allow_algorithm(algorithm)
				.verify(decoded);
			return {
				TokenValidationResult::Status::Valid,
				std::nullopt
			};
		} catch (...)
		{
			return {
				TokenValidationResult::Status::InvalidToken,
				std::nullopt
			};
		}
	}

}