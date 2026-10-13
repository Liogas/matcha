#include <auth/token/TokenValidator.hpp>
#include <auth/token/TokenPolicy.hpp>
#include <exception>
#include <stdexcept>

namespace auth
{
	TokenValidator::TokenValidator(
		const TokenValidatorConfig 	&config,
		auth::JwksProvider 			&jwksProvider,
		ITokenSignatureVerifier		&signatureVerifier,
		IWallClock					&clock
	):
		_config(config),
		_jwksProvider(jwksProvider),
		_signatureVerifier(signatureVerifier),
		_clock(clock)
	{
		if (this->_config.issuer.empty())
			throw std::invalid_argument(
				"Token validator issuer cannot be empty"
			);
		if (this->_config.audience.empty())
			throw std::invalid_argument(
				"Token validator audience cannot be empty"
			);
		if (this->_config.algorithm != TOKEN_ALGORITHM)
			throw std::invalid_argument(
				"Only RS256 token validation is supported"
			);
		if (this->_config.maxLifetime <= std::chrono::seconds::zero())
			throw std::invalid_argument(
				"Token maximum lifetime must be positive"
			);
		if (this->_config.clockSkew < std::chrono::seconds::zero())
			throw std::invalid_argument(
				"Token clock skew cannot be negative"
			);
	}

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
		if (!this->validateTemporalClaims(*decoded))
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

	bool	TokenValidator::validateTemporalClaims(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		if (!decoded.has_payload_claim("exp")
			|| !decoded.has_payload_claim("iat"))
			return false;
		try
		{
			const auto expiration =
				decoded.get_payload_claim("exp").as_date();
			const auto issuedAt =
				decoded.get_payload_claim("iat").as_date();
			const auto now = this->_clock.now();
			const auto skew = this->_config.clockSkew;

			if (expiration <= issuedAt
				|| expiration - issuedAt > this->_config.maxLifetime
				|| issuedAt > now + skew
				|| expiration <= now - skew)
				return false;

			if (decoded.has_payload_claim("nbf"))
			{
				const auto notBefore =
					decoded.get_payload_claim("nbf").as_date();
				if (notBefore > now + skew
					|| notBefore >= expiration)
					return false;
			}
			return true;
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