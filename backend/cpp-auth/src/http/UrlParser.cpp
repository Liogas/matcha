#include <auth/http/UrlParser.hpp>

#include <charconv>

namespace	auth
{
	std::optional<ParsedUrl>
	UrlParser::parse(const std::string &url)
	{
		const auto schemeEnd = url.find("://");
		if (schemeEnd == std::string::npos)
			return std::nullopt;
		const std::string scheme = url.substr(0, schemeEnd);
		if (scheme != "http" && scheme != "https")
			return std::nullopt;
		const std::size_t authorityStart = schemeEnd + 3;
		const std::size_t pathStart = url.find_first_of("/?#", authorityStart);
		const std::size_t authorityEnd =
			pathStart == std::string::npos ? url.size() : pathStart;
		const std::string authority =
			url.substr(
				authorityStart,
				authorityEnd - authorityStart
			);
		if (authority.empty() ||
			authority.find('@') != std::string::npos)
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
			unsigned int portNumber = 0;
			const auto parseResult = std::from_chars(
				port.data(),
				port.data() + port.size(),
				portNumber
			);
			if (parseResult.ec != std::errc{} ||
				parseResult.ptr != port.data() + port.size() ||
				portNumber == 0 ||
				portNumber > 65535)
				return std::nullopt;
		}
		else
			host = authority;
		if (host.empty() ||
			host.find(':') != std::string::npos ||
			host.find_first_of(" \t\r\n\\[]") != std::string::npos)
			return std::nullopt;

		std::string path = "/";
		if (pathStart != std::string::npos && url[pathStart] != '#')
		{
			const auto fragmentStart = url.find('#', pathStart);
			path = url.substr(
				pathStart,
				fragmentStart == std::string::npos
					? std::string::npos
					: fragmentStart - pathStart
			);
			if (!path.empty() && path.front() == '?')
				path.insert(path.begin(), '/');
		}
		return ParsedUrl{
			scheme,
			host,
			port,
			path
		};
	}
}