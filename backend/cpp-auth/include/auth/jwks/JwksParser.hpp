#pragma once

#include "JwksParseResult.hpp"

namespace	auth
{
	class JwksParser
	{
		public:
			virtual ~JwksParser() = default;
			virtual JwksParseResult	parse(
				const std::string &json
			) = 0;
	};
}