#include <jwt-cpp/jwt.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/bn.h>

#include <vector>
#include <stdexcept>
#include <memory>
#include <limits>

#include <auth/crypto/RsaPublicKeyBuilder.hpp>

namespace 
{
	using BIGNUMPtr =
		std::unique_ptr<BIGNUM, decltype(&BN_free)>;
	using OSSLParamBldPtr =
		std::unique_ptr<OSSL_PARAM_BLD, decltype(&OSSL_PARAM_BLD_free)>;
	using OSSLParamPtr =
		std::unique_ptr<OSSL_PARAM, decltype(&OSSL_PARAM_free)>;
	using EVPKeyCtxPtr =
		std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
	using EVPKeyPtr =
		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
}

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
		OSSLParamBldPtr builder(
			OSSL_PARAM_BLD_new(),
			OSSL_PARAM_BLD_free
		);
		if (!builder)
			throw std::runtime_error(
				"unable to create OSSL_PARAM builder"
			);
		if (modulusBytes.size() >
			static_cast<std::size_t>(std::numeric_limits<int>::max())
			|| exponentBytes.size() >
			static_cast<std::size_t>(std::numeric_limits<int>::max())
		)
		{
			throw std::runtime_error("RSA parameter is too large");
		}
		BIGNUMPtr modulus(
			BN_bin2bn(
				reinterpret_cast<const unsigned char *>(
					modulusBytes.data()
				),
				static_cast<int>(modulusBytes.size()),
				nullptr
			),
			BN_free
		);
		BIGNUMPtr exponent(
			BN_bin2bn(
				reinterpret_cast<const unsigned char *>(
					exponentBytes.data()
				),
				static_cast<int>(exponentBytes.size()),
				nullptr
			),
			BN_free
		);
		if (!modulus || !exponent)
			throw std::runtime_error(
				"unable to create RSA parameters"
			);
		if (BN_is_zero(modulus.get())
			|| BN_is_negative(modulus.get())
			|| BN_is_zero(exponent.get())
			|| BN_is_one(exponent.get())
			|| BN_is_negative(exponent.get())
			|| !BN_is_odd(exponent.get())
			|| !BN_is_odd(modulus.get())
			|| BN_num_bits(modulus.get()) < 2048
		)
			throw std::runtime_error(
				"invalid RSA modulus or exponent"
			);
		if (OSSL_PARAM_BLD_push_BN(
				builder.get(),
				OSSL_PKEY_PARAM_RSA_N,
				modulus.get()
			) != 1
			||
			OSSL_PARAM_BLD_push_BN(
				builder.get(),
				OSSL_PKEY_PARAM_RSA_E,
				exponent.get()
			) != 1
		)
		{
			throw std::runtime_error(
				"unable to build RSA parameters"
			);
		}
		OSSLParamPtr params(
			OSSL_PARAM_BLD_to_param(builder.get()),
			OSSL_PARAM_free
		);
		if (!params)
			throw std::runtime_error(
				"unable to create RSA parameters"
			);
		EVPKeyCtxPtr context( 
			EVP_PKEY_CTX_new_from_name(
				nullptr,
				"RSA",
				nullptr
			),
			EVP_PKEY_CTX_free
		);
		if (!context)
			throw std::runtime_error(
				"unable to create RSA key context"
			);
		if (EVP_PKEY_fromdata_init(context.get()) <= 0)
			throw std::runtime_error(
				"unable to initialize RSA key construction"
			);
		EVP_PKEY *rawPublicKey = nullptr;
		const int result = 	EVP_PKEY_fromdata(
								context.get(),
								&rawPublicKey,
								EVP_PKEY_PUBLIC_KEY,
								params.get()
							);
		EVPKeyPtr publicKey(rawPublicKey, EVP_PKEY_free);
		if (result <= 0)
			throw std::runtime_error(
				"unable to construct RSA public key"
			);
		return publicKey.release();
	};
}