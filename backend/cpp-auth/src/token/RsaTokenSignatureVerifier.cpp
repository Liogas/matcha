#include <openssl/evp.h>

#include <memory>
#include <stdexcept>

#include <auth/token/RsaTokenSignatureVerifier.hpp>
#include <auth/crypto/RsaPublicKeyBuilder.hpp>

namespace
{
    using EVPKeyPtr =
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using EVPMdCtxPtr =
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
}

namespace auth
{
	// RsaTokenSignatureVerifier::RsaTokenSignatureVerifier()
	// {}

	bool	RsaTokenSignatureVerifier::verify(
		const std::string &signingInput,
		const std::string &signature,
		const Jwk &jwk
	)
	{
		if (jwk.kty != "RSA" || jwk.alg != "RS256")
			return false;
		EVPKeyPtr publicKey(nullptr, EVP_PKEY_free);
		try
		{
			publicKey.reset(RsaPublicKeyBuilder::build(jwk));
		} catch (const std::runtime_error &)
		{
			return false;
		}
		if (!publicKey)
			return false;
		EVPMdCtxPtr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
		if (!context)
			return false;
		const auto result = EVP_DigestVerifyInit(
			context.get(),
			nullptr,
			EVP_sha256(),
			nullptr,
			publicKey.get()
		);
		if (result != 1)
			throw std::runtime_error(
				"OpenSSL failed to initialize signature verification"
			);
		const auto updateResult = EVP_DigestVerifyUpdate(
			context.get(),
			signingInput.data(),
			signingInput.size()
		);
		if (updateResult != 1)
			throw std::runtime_error(
				"OpenSSL failed to process the token signing input"
			);
		const auto verifyResult = EVP_DigestVerifyFinal(
			context.get(),
			reinterpret_cast<const unsigned char *>(
				signature.data()
			),
			signature.size()
		);
		if (verifyResult == 1)
			return true;
		if (verifyResult == 0)
			return false;
		throw std::runtime_error(
			"OpenSSL failed to verify the token signature"
		);
	}
}