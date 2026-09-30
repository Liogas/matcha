#include <auth/http/CppHttpClient.hpp>
#include <httplib.h>
#include <auth/http/UrlParser.hpp>
#include <iostream>

namespace auth
{
	HttpResult	CppHttpClient::get(
		const std::string &url
	)
	{
		const auto parsedUrl = UrlParser::parse(url);
		if (!parsedUrl.has_value())
			return {
				HttpResult::Status::NetworkError,
				std::nullopt
			};
		if (parsedUrl->scheme == "https")
		{
			std::string baseUrl = "https://" + parsedUrl->host;
    		if (!parsedUrl->port.empty())
				baseUrl += ":" + parsedUrl->port;
			httplib::SSLClient client(baseUrl);
			const auto response = client.Get(parsedUrl->path);
			if (!response)
				return {
					HttpResult::Status::NetworkError,
					std::nullopt
				};
			return {
				HttpResult::Status::Success,
				HttpResponse{
					response->status,
					response->body
				}
			};
		} else if (parsedUrl->scheme == "http")
		{
			std::string baseUrl = "http://" + parsedUrl->host;
			if (!parsedUrl->port.empty())
				baseUrl += ":" + parsedUrl->port;
			httplib::Client client(baseUrl);
			const auto response = client.Get(parsedUrl->path);
			if (!response)
				return {
					HttpResult::Status::NetworkError,
					std::nullopt
				};
			return {
				HttpResult::Status::Success,
				HttpResponse{
					response->status,
					response->body
				}
			};
		}
		return {
			HttpResult::Status::NetworkError,
			std::nullopt
		};
	}
}