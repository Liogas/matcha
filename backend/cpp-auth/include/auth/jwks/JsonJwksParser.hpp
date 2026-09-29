#pragma once

#include "JwksParser.hpp"

namespace
{
	bool	isValidBase64Url(const std::string &value)
	{
		for (const char c : value)
		{
			const bool isUppercase = c >= 'A' && c <= 'Z';
			const bool isLowerCase = c >= 'a' && c <= 'z';
			const bool isDigit = c >= '0' && c <= '9';
			const bool isSpecial = c == '-' || c == '_';
			if (!isUppercase &&
				!isLowerCase &&
				!isDigit &&
				!isSpecial)
				return false;
		}
		return true;
	}
}

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