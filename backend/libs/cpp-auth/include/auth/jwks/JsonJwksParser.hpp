#pragma once

#include "JwksParser.hpp"

namespace	auth
{
	class JsonJwksParser : public JwksParser
	{
		public:
			JwksParseResult	parse(
				const std::string &json
			) override;
	};
}