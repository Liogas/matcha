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
			std::string baseUrl = "https://" + parsedUrl->host;
    		if (!parsedUrl->port.empty())
				baseUrl += ":" + parsedUrl->port;
			std::cout << "[CPP-AUTH BASE URL] ["
						<< baseUrl
						<< "]"
						<< std::endl;

				std::cout << "[CPP-AUTH PATH] ["
						<< parsedUrl->path
						<< "]"
						<< std::endl;
			httplib::SSLClient client(baseUrl);
			// if (this->_config.caCertPath.has_value())
			// 	client.set_ca_cert_path(
			// 		this->_config.caCertPath->c_str()
			// 	);
			client.enable_server_certificate_verification(false); // A RETIRER
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