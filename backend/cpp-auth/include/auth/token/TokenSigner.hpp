#pragma once

#include <string>

namespace auth
{
	class TokenSigner
	{
		public:
			virtual ~TokenSigner() = default;
			virtual std::string	sign(
				const std::string &data
			) = 0;
			virtual std::string	keyId() const = 0;
	};
}