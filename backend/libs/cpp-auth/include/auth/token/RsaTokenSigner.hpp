#pragma once

#include <memory>
#include <openssl/evp.h>

#include <auth/jwks/Jwk.hpp>
#include <auth/token/TokenSigner.hpp>

namespace auth
{
	class RsaTokenSigner : public TokenSigner
	{
		public:
			explicit RsaTokenSigner(
				const std::string &privateKeyPath,
				const std::string &keyId
			);
			std::string sign(
				const std::string &data
			) override;
			std::string	keyId() const override;
			Jwk publicJwk() const;
		private:
			std::string	_keyId;
			std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> _privateKey;
	};
}