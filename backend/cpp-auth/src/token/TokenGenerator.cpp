#include <auth/token/TokenGenerator.hpp>
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>

namespace auth
{
	TokenGenerator::TokenGenerator(
		const TokenGeneratorConfig	&config,
		TokenSigner					&signer,
		IWallClock					&clock
	):
		_config(config),
		_signer(signer),
		_clock(clock)
	{}

	std::string	TokenGenerator::generate(
		const AuthenticatedIdentity &identity
	)
	{
		const nlohmann::json header{
			{"alg", "RS256"},
			{"typ", "JWT"},
			{"kid", this->_signer.keyId()}
		};

		const auto now = this->_clock.now();
		const auto issuedAt =
			std::chrono::duration_cast<std::chrono::seconds>(
				now.time_since_epoch()
			).count();
		const auto expiredAt =
			issuedAt + this->_config.lifetime.count();

		const nlohmann::json payload{
			{"iss", this->_config.issuer},
			{"sub", identity.subject},
			{"aud", this->_config.audience},
			{"iat", issuedAt},
			{"exp", expiredAt}
		};
		const auto encodedHeader =
			jwt::base::encode<jwt::alphabet::base64url>(
				header.dump()
			);
		const auto encodedPayload =
			jwt::base::encode<jwt::alphabet::base64url>(
				payload.dump()
			);
		const auto signingInput =
			encodedHeader + "." + encodedPayload;
		const auto signature = this->_signer.sign(signingInput);
		return signingInput + "." + signature;
	}
}