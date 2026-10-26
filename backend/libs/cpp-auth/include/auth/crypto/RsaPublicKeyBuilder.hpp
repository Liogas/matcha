#include <openssl/evp.h>

#include <auth/jwks/Jwk.hpp>

namespace auth
{
	class RsaPublicKeyBuilder
	{
		public:
			static EVP_PKEY *build(
				const Jwk &jwk
			);
	};
}