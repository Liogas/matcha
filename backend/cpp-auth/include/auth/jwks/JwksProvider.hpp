#pragma once

#include <string>
#include "JwksResult.hpp"

namespace	auth
{
	class	JwksProvider
	{
		public:
			virtual ~JwksProvider() = default;
			virtual JwksResult	getKey(
				const std::string &kid
			) = 0;
	};
}