#include <auth/http/CppHttpClient.hpp>
#include <httplib.h>
#include <auth/http/UrlParser.hpp>
#include <iostream>

namespace auth
{
	CppHttpClient::CppHttpClient(
		const HttpClientConfig &config
	):
		_config(config)
	{}

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
			httplib::SSLClient client(
				parsedUrl->host,
				parsedUrl->port.empty() ? 
					443 : std::stoi(parsedUrl->port)
			);
			if (this->_config.caCertPath.has_value())
				client.set_ca_cert_path(
					this->_config.caCertPath->c_str()
				);
			// client.enable_server_certificate_verification(false); // A RETIRER
			const auto response = client.Get(parsedUrl->path);
			if (!response)
			{
				std::cerr	<< "[HTTPS ERROR] "
							<< httplib::to_string(response.error()) << std::endl;
				return {
					HttpResult::Status::NetworkError,
					std::nullopt
				};
			}
			return {
				HttpResult::Status::Success,
				HttpResponse{
					response->status,
					response->body
				}
			};
		} else if (parsedUrl->scheme == "http")
		{
			httplib::Client client(
				parsedUrl->host,
				parsedUrl->port.empty() 
					? 80 : std::stoi(parsedUrl->port)
			);
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