#include <auth/token/TokenValidator.hpp>

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
		try {
			const auto decoded = jwt::decode(token);
			if (!this->validateAlgorithm(decoded))
				return this->invalidToken();
			if (!this->validateIssuer(decoded))
				return this->invalidToken();
			if (!this->validateAudience(decoded))
				return this->invalidToken();
			if (!this->validateExpiration(decoded))
				return this->invalidToken();
			const auto jwksResult = this->getJwk(decoded);
			if (jwksResult.status == JwksResult::Status::Unavailable)
				return this->verifUnavailable();
			if (jwksResult.status == JwksResult::Status::KeyNotFound)
				return this->invalidToken();
			if (!jwksResult.key.has_value())
				return this->invalidToken();
			if (!this->verifSignature(decoded, jwksResult.key.value()))
				return this->invalidToken();
			return this->validToken(decoded);
		} catch (...)
		{
			return this->invalidToken();
		}
	}

	bool	TokenValidator::validateAlgorithm(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		const std::string algo = decoded.get_algorithm();
		if (algo != this->_config.algorithm)
			return false;
		return true;
	}

	bool	TokenValidator::validateIssuer(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		const std::string issuer =
			decoded.get_payload_claim("iss").as_string();
		if (issuer != this->_config.issuer)
			return false;
		return true;
	}

	bool	TokenValidator::validateAudience(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		const auto audienceClaim =
			decoded.get_payload_claim("aud");
		try
		{
			const std::string audience =
				audienceClaim.as_string();

			if (
				audience.empty()
				|| audience != this->_config.audience
			)
				return false;
		}
		catch (...)
		{
			try
			{
				const auto audiences =
					audienceClaim.as_array();
				bool audienceFound = false;
				for (const auto& audience : audiences)
				{
					if (!audience.is<std::string>())
						return false;
					const std::string value =
						audience.get<std::string>();
					if (!value.empty() && value == this->_config.audience)
					{
						audienceFound = true;
						break;
					}
				}

				if (!audienceFound)
					return false;
			}
			catch (...)
			{
				return false;
			}
		}
		return true;
	}

	bool	TokenValidator::validateExpiration(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		const auto expiration =
			decoded.get_payload_claim("exp").as_date();
		if (expiration <= jwt::date::clock::now())
			return false;
		return true;
	}

	JwksResult	TokenValidator::getJwk(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		const std::string kid =
			decoded.get_header_claim("kid").as_string();
		return this->_jwksProvider.getKey(kid);
	}

	bool	TokenValidator::verifSignature(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded,
		const Jwk &jwk
	)
	{
		const auto publicKey = 
			jwt::helper::create_public_key_from_rsa_components(
				jwk.n,
				jwk.e
			);
		const auto algorithm = jwt::algorithm::rs256(publicKey);
		jwt::verify()
			.allow_algorithm(algorithm)
			.verify(decoded);
		return true;
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
		
		return {
			TokenValidationResult::Status::Valid,
			this->extractClaims(decoded)
		};
	}

	TokenValidationResult	TokenValidator::verifUnavailable()
	{
		return {
			TokenValidationResult::Status::VerificationUnavailable,
			std::nullopt
		};
	}


	AuthenticatedIdentity	TokenValidator::extractClaims(
		const jwt::decoded_jwt<jwt::traits::kazuho_picojson> &decoded
	)
	{
		AuthenticatedIdentity claims;
		claims.issuer = decoded.get_payload_claim("iss").as_string();
		claims.subject = decoded.get_payload_claim("sub").as_string();
		const auto audienceClaim = decoded.get_payload_claim("aud");
		try
		{
			claims.audience.push_back(audienceClaim.as_string());
		} catch (...)
		{
			const auto audiences =
				audienceClaim.as_array();

			for (const auto &audience : audiences)
			{
				claims.audience.push_back(
					audience.get<std::string>()
				);
			}
		}
		return claims;
	}
}