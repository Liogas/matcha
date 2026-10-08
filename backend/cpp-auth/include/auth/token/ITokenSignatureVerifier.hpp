#pragma once

#include <string>

#include <auth/jwks/Jwk.hpp>

namespace auth
{
	class ITokenSignatureVerifier
	{
		public:
			virtual ~ITokenSignatureVerifier() = default;
			virtual bool	verify(
				const std::string &signingInput,
				const std::string &signature,
				const Jwk &jwk
			) = 0;
	};
}