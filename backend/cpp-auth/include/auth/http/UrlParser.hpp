#pragma once

#include <optional>
#include <string>

namespace	auth
{
	struct ParsedUrl
	{
		std::string scheme;
		std::string host;
		std::string port;
		std::string path;
	};

	class UrlParser
	{
		public:
			static std::optional<ParsedUrl> parse(
				const std::string &url
			);
	};
}