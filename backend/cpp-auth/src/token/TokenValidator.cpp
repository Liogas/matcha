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
			const std::string kid =
				decoded.get_header_claim("kid").as_string();
			const auto jwksResult =
				this->_jwksProvider.getKey(kid);
			if (jwksResult.status == auth::JwksResult::Status::Unavailable ||
				(jwksResult.status == auth::JwksResult::Status::Success &&
					!jwksResult.key.has_value()))
				return {
					TokenValidationResult::Status::VerificationUnavailable,
					std::nullopt
				};
			return {
				TokenValidationResult::Status::InvalidToken,
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