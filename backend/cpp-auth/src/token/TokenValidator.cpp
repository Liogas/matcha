#include <auth/token/TokenValidator.hpp>
#include <iostream>
#include <exception>

namespace auth
{
	TokenValidator::TokenValidator(
		const TokenValidatorConfig 	&config,
		auth::JwksProvider 			&jwksProvider,
		ITokenSignatureVerifier		&signatureVerifier
	):
		_config(config),
		_jwksProvider(jwksProvider),
		_signatureVerifier(signatureVerifier)
	{}

	TokenValidationResult	TokenValidator::validate(
		const std::string &token
	)
	{
		using DecodedJwt = jwt::decoded_jwt<jwt::traits::kazuho_picojson>;
		std::optional<DecodedJwt> decoded;
		try {
			decoded.emplace(jwt::decode(token));
		} catch (const std::exception &) {
			return this->invalidToken();
		}
		if (!this->validateAlgorithm(*decoded))
			return this->invalidToken();
		if (!this->validateIssuer(*decoded))
			return this->invalidToken();
		if (!this->validateSubject(*decoded))
			return this->invalidToken();
		if (!this->validateAudience(*decoded))
			return this->invalidToken();
		if (!this->validateExpiration(*decoded))
			return this->invalidToken();
		JwksResult jwksResult; 
		try
		{
			jwksResult = this->getJwk(*decoded);
		} catch (const std::exception &)
		{
			return this->verifUnavailable();
		}
		if (jwksResult.status == JwksResult::Status::Unavailable)
			return this->verifUnavailable();
		if (jwksResult.status == JwksResult::Status::KeyNotFound)
			return this->invalidToken();
		if (!jwksResult.key.has_value())
			return this->invalidToken();
		if (jwksResult.key->alg != decoded->get_algorithm())
			return this->invalidToken();
		try
		{
			if (!this->verifSignature(*decoded, jwksResult.key.value()))
				return this->invalidToken();
		} catch (const std::exception &)
		{
			return this->verifUnavailable();
		}
		return this->validToken(*decoded);
	}

	bool	TokenValidator::validateAlgorithm(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		try
		{
			const std::string algo = decoded.get_algorithm();
			return !algo.empty() && algo == this->_config.algorithm;
		} catch (const std::exception &)
		{
			return false;
		}
	}

	bool	TokenValidator::validateIssuer(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		if (!decoded.has_payload_claim("iss"))
			return false;
		try
		{
			const std::string issuer =
				decoded.get_payload_claim("iss").as_string();
			return issuer == this->_config.issuer;
		} catch (const std::exception &)
		{
			return false;
		}
	}

	bool	TokenValidator::validateAudience(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		if (!decoded.has_payload_claim("aud"))
			return false;
		try
		{
			const auto audienceClaim =
				decoded.get_payload_claim("aud");
			try
			{
				const std::string audience =
					audienceClaim.as_string();
				return !audience.empty() &&
					audience == this->_config.audience;
			} catch (const std::exception &)
			{

			}
			const auto audiences = audienceClaim.as_array();
			for (const auto &audience : audiences)
			{
				if (!audience.is<std::string>())
					return false;
				const std::string value = 
					audience.get<std::string>();
				if (!value.empty() && value == this->_config.audience)
					return true;
			}
			return false;
		}
		catch (const std::exception &)
		{
			return false;
		}
	}

	bool	TokenValidator::validateExpiration(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		if (!decoded.has_payload_claim("exp"))
			return false;
		try
		{
			const auto expiration =
				decoded.get_payload_claim("exp").as_date();
			return expiration > jwt::date::clock::now();
		} catch (const std::exception &)
		{
			return false;
		}
	}

	bool	TokenValidator::validateSubject(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		if (!decoded.has_payload_claim("sub"))
			return false;
		try
		{
			const std::string subject =
				decoded.get_payload_claim("sub").as_string();
			return !subject.empty();
		} catch (const std::exception &)
		{
			return false;
		}
	}

	JwksResult	TokenValidator::getJwk(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		if (!decoded.has_header_claim("kid"))
			return {
				JwksResult::Status::KeyNotFound,
				std::nullopt
			};
		std::string kid;
		try
		{
			kid = decoded.get_header_claim("kid").as_string();
		} catch (const std::exception &)
		{
			return {
				JwksResult::Status::KeyNotFound,
				std::nullopt
			};
		}
		if (kid.empty())
			return {
				JwksResult::Status::KeyNotFound,
				std::nullopt
			};
		return this->_jwksProvider.getKey(kid);
	}

	bool	TokenValidator::verifSignature(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded,
		const Jwk &jwk
	)
	{
		const std::string signingInput =
			decoded.get_header_base64() + "."
			+ decoded.get_payload_base64();
		const std::string signature =
			decoded.get_signature();
		return this->_signatureVerifier.verify(
			signingInput,
			signature,
			jwk
		);
	}

	TokenValidationResult	TokenValidator::invalidToken()
	{
		return {
			TokenValidationResult::Status::InvalidToken,
			std::nullopt
		};
	}

	TokenValidationResult	TokenValidator::validToken(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		auto claims = this->extractClaims(decoded);
		if (!claims.has_value())
			return this->invalidToken();
		return {
			TokenValidationResult::Status::Valid,
			std::move(claims.value())
		};
	}

	TokenValidationResult	TokenValidator::verifUnavailable()
	{
		return {
			TokenValidationResult::Status::VerificationUnavailable,
			std::nullopt
		};
	}


	std::optional<AuthenticatedIdentity>	TokenValidator::extractClaims(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		if (!decoded.has_payload_claim("iss")
			|| !decoded.has_payload_claim("sub")
			|| !decoded.has_payload_claim("aud"))
			return std::nullopt;
		try
		{
			AuthenticatedIdentity claims;
			const auto issuer = decoded.get_payload_claim("iss");
			const auto sub = decoded.get_payload_claim("sub");
			const auto aud = decoded.get_payload_claim("aud");
			if (issuer.get_type() != jwt::json::type::string
				|| sub.get_type() != jwt::json::type::string)
				return std::nullopt;
			claims.issuer = issuer.as_string();
			claims.subject = sub.as_string();
			if (claims.subject.empty())
				return std::nullopt;
			switch(aud.get_type())
			{
				case jwt::json::type::string:
				{
					const auto audience = aud.as_string();
					if (audience.empty())
						return std::nullopt;
					claims.audience.push_back(audience);
					break;
				}
				case jwt::json::type::array:
				{
					const auto audiences = aud.as_array();
					for (const auto &audience : audiences)
					{
						if (!audience.is<std::string>())
							return std::nullopt;
						const auto value = audience.get<std::string>();
						if (value.empty())
							return std::nullopt;
						claims.audience.push_back(value);
					}
					if (claims.audience.empty())
						return std::nullopt;
					break;
				}
				default:
					return std::nullopt;
			}
			return claims;
		} catch (const std::exception &)
		{
			return std::nullopt;
		}
		return std::nullopt;
	}
}