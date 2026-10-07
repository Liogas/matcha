#pragma once

#include <auth/token/TokenGeneratorConfig.hpp>
#include <auth/token/TokenSigner.hpp>
#include <auth/identity/AuthenticatedIdentity.hpp>
#include <auth/time/IWallClock.hpp>

namespace auth
{
	class TokenGenerator
	{
		public:
			TokenGenerator(
				const TokenGeneratorConfig &config,
				TokenSigner &signer,
				IWallClock &clock
			);
			std::string	generate(
				const AuthenticatedIdentity &identity
			);
		private:
			TokenGeneratorConfig	_config;
			TokenSigner				&_signer;
			IWallClock				&_clock;
	};
}