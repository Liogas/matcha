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
	};
}