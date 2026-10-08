#include <openssl/evp.h>
#include <jwt-cpp/jwt.h>

#include <auth/token/RsaTokenSignatureVerifier.hpp>
#include <auth/crypto/RsaPublicKeyBuilder.hpp>

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
		EVP_PKEY *publicKey = 
			RsaPublicKeyBuilder::build(jwk);
		if (!publicKey)
			return false;
		EVP_MD_CTX *context = EVP_MD_CTX_new();
		if (!context)
		{
			EVP_PKEY_free(publicKey);
			return false;
		}
		const auto result = EVP_DigestVerifyInit(
			context,
			nullptr,
			EVP_sha256(),
			nullptr,
			publicKey
		);
		if (result != 1)
		{
			EVP_MD_CTX_free(context);
			EVP_PKEY_free(publicKey);
			return false;
		}
		const auto updateResult = EVP_DigestVerifyUpdate(
			context,
			signingInput.data(),
			signingInput.size()
		);
		if (updateResult != 1)
		{
			EVP_MD_CTX_free(context);
			EVP_PKEY_free(publicKey);
			return false;
		}
		std::string decodedSignature;
		try
		{
			decodedSignature =
				jwt::base::decode<jwt::alphabet::base64url>(
					jwt::base::pad<jwt::alphabet::base64url>(signature)
				);
		} catch (...)
		{
			EVP_MD_CTX_free(context);
			EVP_PKEY_free(publicKey);
			return false;
		}
		const auto verifyResult = EVP_DigestVerifyFinal(
			context,
			reinterpret_cast<const unsigned char *>(
				decodedSignature.data()
			),
			decodedSignature.size()
		);
		EVP_MD_CTX_free(context);
		EVP_PKEY_free(publicKey);
		return verifyResult == 1;
	}
}