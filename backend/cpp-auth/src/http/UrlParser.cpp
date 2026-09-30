#include <auth/http/UrlParser.hpp>

namespace	auth
{
	std::optional<ParsedUrl>
	UrlParser::parse(const std::string &url)
	{
		const auto shemeEnd = url.find("://");
		if (shemeEnd == std::string::npos)
			return std::nullopt;
		const std::string scheme = url.substr(0, shemeEnd);
		if (scheme != "http" && scheme != "https")
			return std::nullopt;
		const std::size_t authorityStart = shemeEnd + 3;
		const std::size_t pathStart = url.find('/', authorityStart);
		const std::size_t authorityEnd =
			pathStart == std::string::npos ? url.size() : pathStart;
		const std::string authority =
			url.substr(
				authorityStart,
				authorityEnd - authorityStart
			);
		if (authority.empty())
			return std::nullopt;
		const auto portSeparator = authority.rfind(':');
		std::string host;
		std::string port;
		if (portSeparator != std::string::npos)
		{
			host = authority.substr(0, portSeparator);
			port = authority.substr(portSeparator + 1);
			if (host.empty() || port.empty())
				return std::nullopt;
		}
		else
			host = authority;
		const std::string path = 
			pathStart == std::string::npos
				? "/" : url.substr(pathStart);
		return ParsedUrl{
			scheme,
			host,
			port,
			path
		};
	}
}