#pragma once

#include <auth/token/TokenSigner.hpp>

namespace auth
{
	class RsaTokenSigner : public TokenSigner
	{
		public:
			explicit RsaTokenSigner(
				const std::string &privateKeyPath
			);
			std::string sign(
				const std::string &data
			) override;
		private:
			std::string	_privateKeyPath;
	};
}