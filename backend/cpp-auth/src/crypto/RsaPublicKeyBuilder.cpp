#include <jwt-cpp/jwt.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>

#include <vector>
#include <stdexcept>

#include <auth/crypto/RsaPublicKeyBuilder.hpp>

namespace auth
{
	EVP_PKEY	*RsaPublicKeyBuilder::build(
		const Jwk	&jwk
	)
	{
		const auto modulusBytes =
			jwt::base::decode<jwt::alphabet::base64url>(
				 jwt::base::pad<jwt::alphabet::base64url>(jwk.n)
			);
		const auto exponentBytes =
			jwt::base::decode<jwt::alphabet::base64url>(
				 jwt::base::pad<jwt::alphabet::base64url>(jwk.e)
			);
		OSSL_PARAM_BLD *builder = OSSL_PARAM_BLD_new();
		if (!builder)
			throw std::runtime_error(
				"unable to create OSSL_PARAM builder"
			);
		BIGNUM	*modulus = BN_bin2bn(
			reinterpret_cast<const unsigned char *>(
				modulusBytes.data()
			),
			modulusBytes.size(),
			nullptr
		);
		BIGNUM	*exponent = BN_bin2bn(
			reinterpret_cast<const unsigned char *>(
				exponentBytes.data()
			),
			exponentBytes.size(),
			nullptr
		);
		if (!modulus || !exponent)
		{
			BN_free(modulus);
			BN_free(exponent);
			OSSL_PARAM_BLD_free(builder);

			throw std::runtime_error(
				"unable to create RSA parameters"
			);
		}
		if (OSSL_PARAM_BLD_push_BN(
				builder,
				OSSL_PKEY_PARAM_RSA_N,
				modulus
			) != 1
			||
			OSSL_PARAM_BLD_push_BN(
				builder,
				OSSL_PKEY_PARAM_RSA_E,
				exponent
			) != 1
		)
		{
			BN_free(modulus);
			BN_free(exponent);
			OSSL_PARAM_BLD_free(builder);
			throw std::runtime_error(
				"unable to build RSA parameters"
			);
		}
		OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(builder);
		if (!params)
		{
			 BN_free(modulus);
			BN_free(exponent);
			OSSL_PARAM_BLD_free(builder);

			throw std::runtime_error(
				"unable to create RSA parameters"
			);
		}
		EVP_PKEY_CTX *context = 
			EVP_PKEY_CTX_new_from_name(
				nullptr,
				"RSA",
				nullptr
			);
		if (!context)
		{
			BN_free(modulus);
			BN_free(exponent);
			OSSL_PARAM_free(params);
			OSSL_PARAM_BLD_free(builder);
			throw std::runtime_error(
				"unable to create RSA key context"
			);
		}
		if (EVP_PKEY_fromdata_init(context) <= 0)
		{
			BN_free(modulus);
			BN_free(exponent);
			OSSL_PARAM_free(params);
			OSSL_PARAM_BLD_free(builder);
			EVP_PKEY_CTX_free(context);
			throw std::runtime_error(
				"unable to initialize RSA key construction"
			);
		}
		EVP_PKEY *publicKey = nullptr;
		if (EVP_PKEY_fromdata(
				context,
				&publicKey,
				EVP_PKEY_PUBLIC_KEY,
				params
			) <= 0
		)
		{
			BN_free(modulus);
			BN_free(exponent);
			OSSL_PARAM_free(params);
			OSSL_PARAM_BLD_free(builder);
			EVP_PKEY_CTX_free(context);
			throw std::runtime_error(
				"unable to construct RSA public key"
			);
		}
		BN_free(modulus);
		BN_free(exponent);
		OSSL_PARAM_free(params);
		OSSL_PARAM_BLD_free(builder);
		EVP_PKEY_CTX_free(context);
		return publicKey;
	};
}