#pragma once

#include <auth/token/ITokenSignatureVerifier.hpp>

namespace auth
{
	class RsaTokenSignatureVerifier : public ITokenSignatureVerifier
	{
		public :
			// RsaTokenSignatureVerifier();
			bool	verify(
				const std::string &signingInput,
				const std::string &signature,
				const Jwk &jwk
			) override;
	};
}