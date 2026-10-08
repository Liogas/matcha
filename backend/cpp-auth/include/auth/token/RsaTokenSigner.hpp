#pragma once

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
		private:
			std::string	_privateKeyPath;
			std::string	_keyId;
	};
}